#include "render/draw_block_manager.h"

#include <glm/gtc/matrix_transform.hpp>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "base/utils/hash.h"

#include "render.pipeline/material_shaders.h"
#include "render/core/camera.h"
#include "scene/scene_system.h"

namespace Mizu
{

struct InternalDrawElement
{
    DrawElement element;
    uint64_t drawable_idx;

    size_t hash;
    size_t pipeline_hash;
    ShaderInstance vertex;
    ShaderInstance fragment;
};

DrawBlockManager::DrawBlockManager(SceneSystem& scene_system) : m_scene_system(scene_system) {}

DrawListHandle DrawBlockManager::create_draw_list(
    DrawListType type,
    [[maybe_unused]] const Frustum& frustum,
    std::vector<uint64_t>& indices)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(
        m_num_draw_lists.load(std::memory_order_acquire) < MAX_DRAW_LISTS,
        "Max number of draw lists reached (max is {})",
        MAX_DRAW_LISTS);

    const size_t idx = m_num_draw_lists.fetch_add(1, std::memory_order_relaxed);

    DrawList& draw_list = m_draw_lists[idx];

    // const std::span<const MeshManagerEntry> meshes = mesh_manager_get().get_meshes();
    const std::span<const SceneDrawableInfo> drawables = m_scene_system.get_drawables();

    std::vector<InternalDrawElement> draw_elements;
    draw_elements.reserve(drawables.size());

    for (size_t drawable_idx = 0; drawable_idx < drawables.size(); ++drawable_idx)
    {
        const SceneDrawableInfo& info = drawables[drawable_idx];

        if (!info.gpu_mesh_record.allocation.handle.is_valid())
        {
            MIZU_LOG_ERROR(
                "Drawable with invalid GPU mesh allocation handle, skipping. Mesh handle: {}, material handle: {}",
                info.mesh_handle.get_id(),
                info.material_handle.get_id());
            continue;
        }

        if (!info.material)
        {
            MIZU_LOG_ERROR(
                "Drawable with no material, skipping. Mesh handle: {}, material handle: {}",
                info.mesh_handle.get_id(),
                info.material_handle.get_id());
            continue;
        }

        const TransformDynamicState& transform_state =
            g_transform_state_manager->rend_get_dynamic_state(info.transform_handle);

        const glm::vec3 translation = transform_state.translation;
        const glm::vec3 rotation = transform_state.rotation;
        const glm::vec3 scale = transform_state.scale;

        glm::mat4 transform{1.0f};
        transform = glm::translate(transform, translation);
        transform = glm::rotate(transform, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, scale);

        // TODO: Need to implement this AABB calculation
        // const BBox& aabb = mesh_entry.mesh->bbox();
        // const BBox transformed_aabb = BBox{
        //    transform * glm::vec4(aabb.min(), 1.0f),
        //    transform * glm::vec4(aabb.max(), 1.0f),
        //};

        FrustumMask mask;
        if (type == DrawListType::Shadow)
        {
            // TODO: HACK, to include all meshes in cascaded shadow maps
            mask = FrustumMask{
                .top = false,
                .bottom = false,
                .left = false,
                .right = false,
                .near = false,
                .far = false,
            };
        }

        // TODO: TEMPORAL
        // if (!frustum.is_inside_frustum(transformed_aabb))
        //{
        //    continue;
        //}

        const size_t material_hash = info.material->get_material_hash();
        const size_t mesh_hash =
            hash_compute(info.gpu_mesh_record.allocation.vertex_offset, info.gpu_mesh_record.allocation.index_offset);

        PBROpaqueShaderVS vertex_shader{};
        PBROpaqueShaderFS fragment_shader{};

        const auto vertex_instance = vertex_shader.get_instance();
        const auto fragment_instance = fragment_shader.get_instance();

        const size_t pipeline_hash = hash_compute(
            vertex_instance.virtual_path,
            vertex_instance.entry_point,
            fragment_instance.virtual_path,
            fragment_instance.entry_point);

        const size_t hash = hash_compute(pipeline_hash, material_hash, mesh_hash);

        DrawElement draw_element{};
        draw_element.gpu_mesh_draw = info.gpu_mesh_draw;
        draw_element.material_buffer_slot = info.material_buffer_slot;
        draw_element.material = info.material;
        draw_element.instance_count = 1;
        draw_element.transform_offset = 0;

        InternalDrawElement internal_draw_element{};
        internal_draw_element.element = draw_element;
        internal_draw_element.drawable_idx = drawable_idx;
        internal_draw_element.hash = hash;
        internal_draw_element.pipeline_hash = pipeline_hash;
        internal_draw_element.vertex = vertex_instance;
        internal_draw_element.fragment = fragment_instance;

        draw_elements.push_back(internal_draw_element);
    }

    std::sort(
        draw_elements.begin(), draw_elements.end(), [](const InternalDrawElement& a, const InternalDrawElement& b) {
            return a.hash < b.hash;
        });

    size_t i = 0;
    while (i < draw_elements.size())
    {
        const InternalDrawElement& internal = draw_elements[i];

        DrawElement element = internal.element;
        element.transform_offset = i;

        indices[i] = internal.drawable_idx;

        i += 1;

        while (i < draw_elements.size() && internal.hash == draw_elements[i].hash)
        {
            element.instance_count += 1;
            indices[i] = draw_elements[i].drawable_idx;

            i += 1;
        }

        if (draw_list.num_blocks == 0
            || draw_list.blocks[draw_list.num_blocks - 1].pipeline_hash != internal.pipeline_hash
            || draw_list.blocks[draw_list.num_blocks - 1].num_elements >= DrawBlock::MAX_DRAW_ELEMENTS)
        {
            MIZU_ASSERT(
                draw_list.num_blocks + 1 < DrawList::MAX_DRAW_BLOCKS,
                "Reached maximum number of draw blocks in the list. The maximum is {}",
                DrawList::MAX_DRAW_BLOCKS);

            DrawBlock block{};
            block.num_elements = 0;
            block.pipeline_hash = internal.pipeline_hash;
            block.vertex_instance = internal.vertex;
            block.fragment_instance = internal.fragment;

            draw_list.blocks[draw_list.num_blocks] = block;
            draw_list.num_blocks += 1;
        }

        DrawBlock& block = draw_list.blocks[draw_list.num_blocks - 1];
        block.elements[block.num_elements] = element;
        block.num_elements += 1;
    }

    return static_cast<DrawListHandle>(idx);
}

const DrawList& DrawBlockManager::get_draw_list(DrawListHandle handle) const
{
    MIZU_ASSERT(
        handle < m_num_draw_lists.load(std::memory_order_acquire),
        "Invalid DrawListHandle, requested handle {} but there are only {} draw lists",
        handle,
        m_num_draw_lists.load());

    return m_draw_lists[handle];
}

void DrawBlockManager::reset()
{
    MIZU_PROFILE_SCOPED;

    m_num_draw_lists.store(0);

    for (DrawList& list : m_draw_lists)
    {
        list.num_blocks = 0;

        for (DrawBlock& block : list.blocks)
        {
            block.num_elements = 0;
        }
    }
}

} // namespace Mizu
