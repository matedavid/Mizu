#include "draw_block_manager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <map>

#include "application/thread_sync.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "base/utils/hash.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/rhi_helpers.h"
#include "renderer/camera.h"
#include "renderer/material/material.h"
#include "renderer/model/mesh.h"

#include "state_manager/static_mesh_state_manager.h"

namespace Mizu
{

struct InternalDrawElement
{
    DrawElement element;
    uint64_t handle_idx;

    size_t hash;
    size_t pipeline_hash;
    // std::shared_ptr<Shader> vertex;
    ShaderInstance vertex;
    // std::shared_ptr<Shader> fragment;
    ShaderInstance fragment;
};

DrawListHandle DrawBlockManager::create_draw_list(
    DrawListType type,
    const Frustum& frustum,
    std::vector<uint64_t>& indices)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(
        m_num_draw_lists.load(std::memory_order_acquire) < MAX_DRAW_LISTS,
        "Max number of draw lists reached (max is {})",
        MAX_DRAW_LISTS);

    const size_t idx = m_num_draw_lists.fetch_add(1, std::memory_order_relaxed);

    DrawList& draw_list = m_draw_lists[idx];

    const StaticMeshStateManager::IteratorWrapper& wrapper = g_static_mesh_state_manager->rend_iterator();

    std::vector<InternalDrawElement> draw_elements;
    draw_elements.reserve(wrapper.size());

    for (size_t handle_idx = 0; handle_idx < wrapper.size(); ++handle_idx)
    {
        const StaticMeshHandle& handle = *(wrapper.begin() + handle_idx);
        const StaticMeshStaticState& ss = g_static_mesh_state_manager->rend_get_static_state(handle);

        if (ss.mesh == nullptr)
        {
            MIZU_LOG_ERROR("StaticMesh with handle: '{}' does not have a valid mesh", handle.get_internal_id());
            continue;
        }

        if (ss.material == nullptr)
        {
            MIZU_LOG_ERROR("StaticMesh with handle: '{}' does not have a valid material", handle.get_internal_id());
            continue;
        }

        const glm::vec3 translation = ss.transform_handle->get_translation();
        const glm::vec3 rotation = ss.transform_handle->get_rotation();
        const glm::vec3 scale = ss.transform_handle->get_scale();

        glm::mat4 transform{1.0f};
        transform = glm::translate(transform, translation);
        transform = glm::rotate(transform, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, scale);

        const BBox& aabb = ss.mesh->bbox();
        const BBox transformed_aabb = BBox{
            transform * glm::vec4(aabb.min(), 1.0f),
            transform * glm::vec4(aabb.max(), 1.0f),
        };

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

        if (!frustum.is_inside_frustum(transformed_aabb))
        {
            continue;
        }

        const size_t pipeline_hash = ss.material->get_pipeline_hash();
        const size_t material_hash = ss.material->get_material_hash();

        const size_t hash = hash_compute(pipeline_hash, material_hash, ss.mesh.get());

        DrawElement draw_element{};
        draw_element.mesh = ss.mesh;
        draw_element.material = ss.material;
        draw_element.instance_count = 1;
        draw_element.transform_offset = 0;

        const MaterialShaderInstance& material_shader = ss.material->get_shader_instance();
        ShaderInstance fragment_instance{
            material_shader.virtual_path,
            material_shader.entry_point,
            ShaderType::Fragment,
            ShaderCompilationEnvironment{}};

        InternalDrawElement internal_draw_element{};
        internal_draw_element.element = draw_element;
        internal_draw_element.handle_idx = handle_idx;
        internal_draw_element.hash = hash;
        internal_draw_element.pipeline_hash = pipeline_hash;
        internal_draw_element.vertex = PBROpaqueShaderVS{}.get_instance();
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

        indices[i] = internal.handle_idx;

        i += 1;

        while (i < draw_elements.size() && internal.hash == draw_elements[i].hash)
        {
            element.instance_count += 1;
            indices[i] = draw_elements[i].handle_idx;

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