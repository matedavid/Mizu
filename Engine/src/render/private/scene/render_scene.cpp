#include "scene/render_scene.h"

#include "base/debug/assert.h"
#include "shader/shader_asset.h"

#include "registries/transform_registry.h"
#include "render.pipeline/scene_renderer_shaders.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "resources/material_system.h"
#include "spdlog/details/windows_include.h"
#include "streaming/mesh_streaming_manager.h"
#include "streaming/streaming_planner.h"

namespace Mizu
{

static MeshStreamingManager* get_mesh_streaming_manager(StreamingPlanner2& planner)
{
    MeshStreamingManager* manager = planner.get_streaming_manager<MeshStreamingManager>();
    MIZU_VERIFY(manager != nullptr, "StreamingPlanner requires a MeshStreamingManager");

    return manager;
}

static GpuMeshDrawPayload create_gpu_mesh_draw_payload(const MeshStreamingResidency& residency)
{
    // TODO: need to really revisit if we need uint64_t for vertex_count as we do the cast
    return {
        .vertex_count = static_cast<uint32_t>(residency.metadata.vertex_count),
        .index_count = static_cast<uint32_t>(residency.metadata.index_count),
        .first_vertex = static_cast<uint32_t>(
            residency.allocation_handle.vertex_offset / residency.metadata.get_vertex_stride_bytes()),
        .first_index = static_cast<uint32_t>(
            residency.allocation_handle.index_offset / residency.metadata.get_index_stride_bytes()),
    };
}

RenderScene::RenderScene(StreamingPlanner2& streaming_planner, MaterialSystem& material_system)
    : m_streaming_planner(streaming_planner)
    , m_mesh_streaming_manager(*get_mesh_streaming_manager(m_streaming_planner))
    , m_material_system(material_system)
{
    g_static_mesh_state_manager->register_rend_consumer(this);
}

RenderScene::~RenderScene()
{
    g_static_mesh_state_manager->unregister_rend_consumer(this);
}

void RenderScene::update()
{
    for (const MeshStreamingUpdate& update : m_mesh_streaming_manager.get_updates())
    {
        handle_mesh_update(update);
    }

    for (const MaterialUpdate& update : m_material_system.get_updates())
    {
        handle_material_update(update);
    }
}

void RenderScene::rend_on_create(
    StaticMeshHandle handle,
    const StaticMeshStaticState& ss,
    const StaticMeshDynamicState&)
{
    const uint64_t slot_index = handle.get_internal_id();

    RenderableSlot& slot = m_slots[slot_index];
    MIZU_ASSERT(!slot.occupied, "Trying to create a renderable on an already occupied slot");

    slot = RenderableSlot{
        .occupied = true,
        .slot_index = slot_index,
        .static_mesh_handle = handle,
        .ss = ss,
        .mesh_dependency_chain = {},
        .material_dependency_chain = {},
        .is_mesh_drawable = is_mesh_drawable(ss.mesh_handle),
        .is_material_drawable = is_material_drawable(ss.material_handle),
        .is_drawable = false,
        .drawable_slot_index = INVALID_SLOT,
    };

    add_dependency(
        m_mesh_dependency_chain_head_map, ss.mesh_handle, &RenderableSlot::mesh_dependency_chain, slot_index);
    add_dependency(
        m_material_dependency_head_map, ss.material_handle, &RenderableSlot::material_dependency_chain, slot_index);

    if (slot.is_mesh_drawable && slot.is_material_drawable)
    {
        try_transition_to_drawable(slot);
    }

    m_mesh_streaming_manager.add_usage({
        .streaming_handle = handle.get_internal_id(),
        .mesh_handle = ss.mesh_handle,
        .transform_handle = ss.transform_handle,
    });
    m_material_system.register_material(
        ss.material_handle, ss.transform_handle, ss.mesh_handle, handle.get_internal_id());
}

void RenderScene::rend_on_update(StaticMeshHandle, const StaticMeshDynamicState&)
{
    // StaticMeshStateManager has an empty DynamicState
}

void RenderScene::rend_on_destroy(StaticMeshHandle handle)
{
    const uint64_t slot_index = handle.get_internal_id();

    RenderableSlot& slot = m_slots[slot_index];
    MIZU_ASSERT(slot.occupied, "Trying to destroy a renderable on a non occupied slot");

    remove_dependency(
        m_mesh_dependency_chain_head_map, slot.ss.mesh_handle, &RenderableSlot::mesh_dependency_chain, slot_index);
    remove_dependency(
        m_material_dependency_head_map,
        slot.ss.material_handle,
        &RenderableSlot::material_dependency_chain,
        slot_index);

    if (slot.is_drawable && slot.drawable_slot_index != INVALID_SLOT)
    {
        free_drawable_slot(slot.drawable_slot_index);
        slot.drawable_slot_index = INVALID_SLOT;
    }

    slot.occupied = false;

    m_mesh_streaming_manager.remove_usage(handle.get_internal_id());
    m_material_system.unregister_material(slot.ss.material_handle, handle.get_internal_id());
}

void RenderScene::handle_mesh_update(const MeshStreamingUpdate& update)
{
    const auto mesh_head_it = m_mesh_dependency_chain_head_map.find(update.handle);
    if (mesh_head_it == m_mesh_dependency_chain_head_map.end())
        return;

    size_t slot_index = mesh_head_it->second;
    while (slot_index != INVALID_SLOT)
    {
        RenderableSlot& slot = m_slots[slot_index];

        // Advance now so that we can call continue
        slot_index = slot.mesh_dependency_chain.next;

        const bool mesh_transitioning_to_drawable = !slot.is_mesh_drawable && update.resident;
        const bool transitioning_to_undrawable = slot.is_drawable && !update.resident;

        slot.is_mesh_drawable = update.resident;

        // If transitioning to drawable, try and transition slot to drawable
        if (mesh_transitioning_to_drawable && slot.is_material_drawable)
        {
            slot.is_mesh_drawable = true;
            try_transition_to_drawable(slot);

            continue;
        }

        // If slot is drawable and mesh is becoming non-resident, free drawable slot
        if (transitioning_to_undrawable)
        {
            free_drawable_slot(slot.drawable_slot_index);

            slot.drawable_slot_index = INVALID_SLOT;
            slot.is_drawable = false;

            continue;
        }

        // If slot is drawable and update is for resident mesh, just update the drawable
        if (slot.is_drawable)
        {
            SceneDrawable& drawable = m_drawables[slot.drawable_slot_index];
            drawable.gpu_mesh_draw = create_gpu_mesh_draw_payload(update.residency);
        }

        // If slot is waiting for material to become drawable, do nothing
    }
}

void RenderScene::handle_material_update(const MaterialUpdate& update)
{
    const auto material_head_it = m_material_dependency_head_map.find(update.handle);
    if (material_head_it == m_material_dependency_head_map.end())
        return;

    size_t slot_index = material_head_it->second;
    while (slot_index != INVALID_SLOT)
    {
        RenderableSlot& slot = m_slots[slot_index];

        // Advance now so that we can call continue
        slot_index = slot.material_dependency_chain.next;

        const bool material_transitioning_to_drawable = !slot.is_material_drawable && update.drawable;
        const bool transitioning_to_undrawable = slot.is_drawable && !update.drawable;

        slot.is_material_drawable = update.drawable;

        // If transitioning to drawable, try and transition slot to drawable
        if (material_transitioning_to_drawable && slot.is_mesh_drawable)
        {
            slot.is_material_drawable = true;
            try_transition_to_drawable(slot);

            continue;
        }

        // If slot is drawable and mesh is becoming non-resident, free drawable slot
        if (transitioning_to_undrawable)
        {
            free_drawable_slot(slot.drawable_slot_index);

            slot.drawable_slot_index = INVALID_SLOT;
            slot.is_drawable = false;

            continue;
        }

        // If slot is drawable and update is for resident mesh, just update the drawable
        if (slot.is_drawable)
        {
            SceneDrawable& drawable = m_drawables[slot.drawable_slot_index];
            drawable.material_buffer_offset = update.binding.buffer_slot;
        }

        // If slot is waiting for mesh to become drawable, do nothing
    }
}

bool RenderScene::try_transition_to_drawable(RenderableSlot& slot)
{
    if (slot.is_drawable)
        return true;

    if (slot.is_mesh_drawable && slot.is_material_drawable)
    {
        const uint32_t transform_index = transform_registry_get().get_transform_slot_index(slot.ss.transform_handle);

        const std::optional<MaterialBinding> material_binding =
            m_material_system.get_material_binding(slot.ss.material_handle);
        MIZU_ASSERT(material_binding.has_value(), "Failed to get material binding slot");

        const std::optional<MeshStreamingResidency> mesh_residency =
            m_mesh_streaming_manager.get_mesh_residency(slot.ss.mesh_handle);
        MIZU_ASSERT(mesh_residency.has_value(), "Failed to get mesh residency");

        // TODO: Vertex hardcoded until we select vertex shader based on mesh requirements.
        const ShaderAssetHandle vertex_shader_handle = get_shader_declaration_asset_handle(PbrOpaqueMaterialShaderVS{});
        const ShaderAssetHandle fragment_shader_Handle = material_binding->shader_handle;

        const DrawClassId class_id = m_draw_class_registry.acquire(
            DrawClassDesc{
                .vertex = vertex_shader_handle,
                .fragment = fragment_shader_Handle,
            });

        const SceneDrawable drawable{
            .owner_slot = slot.slot_index,
            .mesh_handle = slot.ss.mesh_handle,
            .material_handle = slot.ss.material_handle,
            .gpu_mesh_draw = create_gpu_mesh_draw_payload(*mesh_residency),
            .bounding_box = mesh_residency->metadata.bounding_box,
            .transform_slot_index = transform_index,
            .material_buffer_offset = material_binding->buffer_slot,
            .class_id = class_id,
            .flags = DrawableFlags::None,
        };

        slot.is_drawable = true;
        slot.drawable_slot_index = allocate_drawable_slot(drawable);
    }

    return slot.is_drawable;
}

bool RenderScene::is_mesh_drawable(MeshAssetHandle handle) const
{
    return m_mesh_streaming_manager.is_drawable(handle);
}

bool RenderScene::is_material_drawable(MaterialAssetHandle handle) const
{
    return m_material_system.is_drawable(handle);
}

size_t RenderScene::allocate_drawable_slot(const SceneDrawable& info)
{
    const size_t index = m_drawables.size();
    m_drawables.push_back(info);

    return index;
}

void RenderScene::free_drawable_slot(size_t index)
{
    MIZU_ASSERT(index < m_drawables.size(), "Drawable index {} is out of range", index);

    m_draw_class_registry.release(m_drawables[index].class_id);

    const size_t last_index = m_drawables.size() - 1;
    if (index != last_index)
    {
        const SceneDrawable moved = m_drawables[last_index];
        m_drawables[index] = moved;

        const uint64_t moved_handle_id = moved.owner_slot;
        MIZU_ASSERT(moved_handle_id < m_slots.size(), "Moved drawable handle id {} out of range", moved_handle_id);

        RenderableSlot& moved_slot = m_slots[moved_handle_id];
        MIZU_ASSERT(moved_slot.occupied, "Moved drawable slot must be occupied");
        moved_slot.drawable_slot_index = index;
    }

    const auto diff_last = static_cast<std::ptrdiff_t>(last_index);
    m_drawables.erase(std::next(m_drawables.begin(), diff_last), std::next(m_drawables.begin(), diff_last + 1));
}

template <AssetHandleType AssetHandleT>
void RenderScene::add_dependency(
    std::unordered_map<AssetHandleT, size_t>& dependency_head_map,
    AssetHandleT handle,
    DependencyChain RenderableSlot::* chain_member,
    size_t new_slot)
{
    RenderableSlot& slot = m_slots[new_slot];
    DependencyChain& chain = slot.*chain_member;

    auto [chain_head_it, inserted] = dependency_head_map.try_emplace(handle, new_slot);
    if (!inserted)
    {
        DependencyChain& head_chain = m_slots[chain_head_it->second].*chain_member;
        head_chain.prev = new_slot;

        chain.next = chain_head_it->second;

        chain_head_it->second = new_slot;
    }
}

template <AssetHandleType AssetHandleT>
void RenderScene::remove_dependency(
    std::unordered_map<AssetHandleT, size_t>& dependency_head_map,
    AssetHandleT handle,
    DependencyChain RenderableSlot::* chain_member,
    size_t removing_slot)
{
    RenderableSlot& slot = m_slots[removing_slot];
    DependencyChain& chain = slot.*chain_member;

    const auto head_it = dependency_head_map.find(handle);
    if (head_it == dependency_head_map.end())
        return;

    const size_t prev_slot_idx = chain.prev;
    const size_t next_slot_idx = chain.next;

    if (prev_slot_idx != INVALID_SLOT)
    {
        RenderableSlot& prev_slot = m_slots[prev_slot_idx];
        DependencyChain& prev_chain = prev_slot.*chain_member;

        prev_chain.next = next_slot_idx;
    }

    if (next_slot_idx != INVALID_SLOT)
    {
        RenderableSlot& next_slot = m_slots[next_slot_idx];
        DependencyChain& next_chain = next_slot.*chain_member;

        next_chain.prev = prev_slot_idx;
    }

    if (head_it->second == removing_slot)
    {
        head_it->second = prev_slot_idx;
    }

    chain.next = INVALID_SLOT;
    chain.prev = INVALID_SLOT;
}

} // namespace Mizu