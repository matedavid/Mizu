#include "scene/scene_system.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render.pipeline/scene_shaders.h"
#include "render/runtime/renderer.h"
#include "render/systems/pipeline_cache.h"
#include "resources/residency_system.h"

namespace Mizu
{

SceneSystem::SceneSystem(MeshResidencySystem& mesh_residency_system, MaterialResidencySystem& material_residency_system)
    : m_mesh_residency_system(mesh_residency_system)
    , m_material_residency_system(material_residency_system)
{
    g_transform_state_manager->register_rend_consumer(this);

    constexpr uint32_t TRANSFORM_INFO_BUFFER_NUM = TransformConfig::MaxNumHandles * 2;

    m_transform_infos.resize(TRANSFORM_INFO_BUFFER_NUM);

    for (uint32_t i = 0; i < TRANSFORM_INFO_BUFFER_NUM; ++i)
        m_free_transform_slots.push(TRANSFORM_INFO_BUFFER_NUM - i - 1);

    std::fill(m_transform_slot_indices.begin(), m_transform_slot_indices.end(), INVALID_SLOT_U32);

    BufferDescription transform_info_buffer_desc{};
    transform_info_buffer_desc.size = sizeof(TransformInfo) * TRANSFORM_INFO_BUFFER_NUM;
    transform_info_buffer_desc.stride = sizeof(TransformInfo);
    transform_info_buffer_desc.usage = BufferUsageBits::ShaderResource | BufferUsageBits::UnorderedAccess;
    transform_info_buffer_desc.name = "SceneSystem::TransformInfoBuffer";

    m_transform_info_buffer = g_render_device->create_buffer(transform_info_buffer_desc);
}

SceneSystem::~SceneSystem()
{
    g_transform_state_manager->unregister_rend_consumer(this);
}

void SceneSystem::update(const ResourceEventStream& stream, uint64_t frame_num)
{
    MIZU_PROFILE_SCOPED;

    consume_renderable_events(stream, frame_num);
    consume_mesh_residency_events(stream);
    consume_material_residency_events(stream);
    track_transform_evictions(frame_num);
}

void SceneSystem::add_transform_publish_pass(RenderGraphBuilder& builder, FrameLinearAllocator& linear_allocator)
{
    if (m_pending_transform_updates.empty())
        return;

    const uint64_t pending_updates = m_pending_transform_updates.size();

    const FrameAllocation transform_info_buffer_allocation =
        linear_allocator.allocate_structured<PendingTransformUpdate>(pending_updates);
    transform_info_buffer_allocation.upload(std::span(m_pending_transform_updates.data(), pending_updates));

    struct PublishInfo
    {
        RenderGraphResource transform_buffer;
    };

    const RenderGraphResource transform_buffer_resource = builder.register_external_buffer(
        m_transform_info_buffer, {BufferResourceState::ShaderReadOnly, BufferResourceState::ShaderReadOnly});

    builder.add_pass<PublishInfo>(
        "SceneSystem::TransformPublishPass",
        [&](RenderGraphPassBuilder& pass, PublishInfo& info) {
            pass.set_hint(RenderGraphPassHint::Compute);

            info.transform_buffer = pass.write(transform_buffer_resource);
        },
        [=](CommandBuffer& command, const PublishInfo& info, const RenderGraphPassResources& resources) {
            struct PushConstant
            {
                uint64_t update_count;
            } push_constant;

            push_constant.update_count = pending_updates;

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TransformPublishLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(0, 1, ShaderType::Compute)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::shared_ptr<BufferResource>& transform_buffer = resources.get_buffer(info.transform_buffer);

            std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, transform_info_buffer_allocation.view),
                WriteDescriptor::StructuredBufferUav(0, BufferResourceView::create(transform_buffer)),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                TransformPublishLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            const auto pipeline = get_compute_pipeline(PublishTransformsShaderCS{});
            command.bind_pipeline(pipeline);

            command.bind_descriptor_set(descriptor_set, 0);
            command.push_constant(push_constant);

            const glm::uvec3 group_count =
                compute_group_count({pending_updates, 1, 1}, {PublishTransformsShaderCS::GROUP_SIZE, 1, 1});
            command.dispatch(group_count);
        });

    m_pending_transform_updates.clear();
}

void SceneSystem::consume_renderable_events(const ResourceEventStream& stream, uint64_t frame_num)
{
    MIZU_PROFILE_SCOPED;

    for (const RenderableEvent& event : stream.get_renderable_events())
    {
        if (!event.static_mesh_handle.is_valid())
            continue;

        switch (event.type)
        {
        case RenderableEventType::Create:
            handle_renderable_create_event(event);
            break;
        case RenderableEventType::Update:
            // Does nothing as StaticMeshStateManager does not have a dynamic state
            break;
        case RenderableEventType::Destroy:
            handle_renderable_destroy_event(event, frame_num);
            break;
        }
    }
}

void SceneSystem::consume_mesh_residency_events(const ResourceEventStream& stream)
{
    MIZU_PROFILE_SCOPED;

    for (const MeshResidencyEvent& event : stream.get_mesh_residency_events())
    {
        switch (event.type)
        {
        case ResidencySystemEventType::Loading:
            // Does nothing as SceneSystem only cares about GPU residency
            break;
        case ResidencySystemEventType::GpuResident:
            handle_mesh_residency_gpu_resident_event(event);
            break;
        case ResidencySystemEventType::Evicting:
            // We don't handle eviction in SceneSystem
            break;
        }
    }
}

void SceneSystem::consume_material_residency_events(const ResourceEventStream& stream)
{
    MIZU_PROFILE_SCOPED;

    for (const MaterialResidencyEvent& event : stream.get_material_residency_events())
    {
        switch (event.type)
        {
        case ResidencySystemEventType::Loading:
            // Does nothing as SceneSystem only cares about GPU residency
            break;
        case ResidencySystemEventType::GpuResident:
            handle_material_residency_gpu_resident_event(event);
            break;
        case ResidencySystemEventType::Evicting:
            // We don't handle eviction in SceneSystem
            break;
        }
    }
}

void SceneSystem::track_transform_evictions(uint64_t frame_num)
{
    constexpr uint64_t EVICTION_FRAMES = 10;

    auto it = m_pending_transform_evictions.begin();

    while (it != m_pending_transform_evictions.end())
    {
        const PendingTransformEviction& info = *it;
        MIZU_ASSERT(info.slot_idx != INVALID_SLOT_U32, "Invalid slot idx");

        if (frame_num - info.last_frame_num > EVICTION_FRAMES)
        {
            free_transform_slot(info.slot_idx);
            it = m_pending_transform_evictions.erase(it);
        }
        else
        {
            it = std::next(it);
        }
    }
}

void SceneSystem::handle_renderable_create_event(const RenderableEvent& event)
{
    const uint64_t handle_id = event.static_mesh_handle.get_internal_id();

    RenderableSlot& slot = m_slots[handle_id];
    MIZU_ASSERT(!slot.occupied, "Trying to create a renderable on an already occupied slot");

    slot = RenderableSlot{
        .occupied = true,
        .drawable_info =
            SceneDrawableInfo{
                .static_mesh_handle = event.static_mesh_handle,
                .transform_handle = event.transform_handle,
                .mesh_handle = event.mesh_handle,
                .material_handle = event.material_handle,
                .transform_slot_index = allocate_transform_slot(event.transform_handle),
            },
        .mesh_resident = is_mesh_resident(event.mesh_handle),
        .material_resident = is_material_resident(event.material_handle),
    };

    const TransformDynamicState& ds = g_transform_state_manager->rend_get_dynamic_state(event.transform_handle);
    m_pending_transform_updates.push_back({
        .new_transform = build_transform_info(ds),
        .dst_slot = slot.drawable_info.transform_slot_index,
    });

    if (!slot.mesh_resident)
    {
        link_mesh_dependency(event.mesh_handle, slot.mesh_dependency, handle_id);
    }

    if (!slot.material_resident)
    {
        link_material_dependency(event.material_handle, slot.material_dependency, handle_id);
    }

    if (slot.mesh_resident && slot.material_resident)
    {
        try_transition_to_drawable(handle_id);
    }
}

void SceneSystem::handle_renderable_destroy_event(const RenderableEvent& event, uint64_t frame_num)
{
    const uint64_t handle_id = event.static_mesh_handle.get_internal_id();

    RenderableSlot& slot = m_slots[handle_id];
    MIZU_ASSERT(slot.occupied, "Trying to destroy a renderable on a non occupied slot");

    if (slot.drawable)
    {
        free_drawable_slot(slot.drawable_slot_index);
    }
    else
    {
        if (!slot.mesh_resident)
        {
            unlink_mesh_dependency(event.mesh_handle, handle_id);
        }

        if (!slot.material_resident)
        {
            unlink_material_dependency(event.material_handle, handle_id);
        }
    }

    m_pending_transform_evictions.push_back({
        .slot_idx = slot.drawable_info.transform_slot_index,
        .last_frame_num = frame_num,
    });

    slot = RenderableSlot{.occupied = false};
}

void SceneSystem::handle_mesh_residency_gpu_resident_event(const MeshResidencyEvent& event)
{
    const auto it = m_mesh_dependency_head_map.find(event.mesh_handle);
    if (it == m_mesh_dependency_head_map.end())
        return;

    size_t index = it->second;

    while (index != INVALID_SLOT)
    {
        RenderableSlot& slot = m_slots[index];
        MIZU_ASSERT(slot.occupied, "Mesh dependency slot should be occupied");

        slot.mesh_resident = true;

        try_transition_to_drawable(index);

        index = slot.mesh_dependency.next;

        slot.mesh_dependency.next = INVALID_SLOT;
        slot.mesh_dependency.prev = INVALID_SLOT;
    }

    m_mesh_dependency_head_map.erase(event.mesh_handle);
}

void SceneSystem::handle_material_residency_gpu_resident_event(const MaterialResidencyEvent& event)
{
    const auto it = m_material_dependency_head_map.find(event.material_handle);
    if (it == m_material_dependency_head_map.end())
        return;

    size_t index = it->second;

    while (index != INVALID_SLOT)
    {
        RenderableSlot& slot = m_slots[index];
        MIZU_ASSERT(slot.occupied, "Material dependency slot should be occupied");

        slot.material_resident = true;

        try_transition_to_drawable(index);

        index = slot.material_dependency.next;

        slot.material_dependency.next = INVALID_SLOT;
        slot.material_dependency.prev = INVALID_SLOT;
    }

    m_material_dependency_head_map.erase(event.material_handle);
}

bool SceneSystem::try_transition_to_drawable(size_t slot_idx)
{
    RenderableSlot& slot = m_slots[slot_idx];
    if (slot.drawable)
        return false;

    if (slot.mesh_resident && slot.material_resident)
    {
        const std::optional<GpuMeshResidentRecord> gpu_mesh_record =
            m_mesh_residency_system.get_gpu_resident_record(slot.drawable_info.mesh_handle);
        const std::optional<uint32_t> material_buffer_offset =
            m_material_residency_system.get_material_buffer_offset(slot.drawable_info.material_handle);

        if (!gpu_mesh_record.has_value() || !material_buffer_offset.has_value())
        {
            MIZU_LOG_ERROR(
                "Failed to get residency info for mesh handle {} or material handle {} while transitioning to "
                "drawable",
                slot.drawable_info.mesh_handle.get_id(),
                slot.drawable_info.material_handle.get_id());
            return false;
        }

        slot.drawable_info.gpu_mesh_record = *gpu_mesh_record;

        const uint64_t index_element_size = gpu_mesh_record->payload.get_index_element_size_bytes();

        MIZU_ASSERT(index_element_size > 0, "Mesh index element size must be non-zero");
        MIZU_ASSERT(
            gpu_mesh_record->allocation.index_offset % index_element_size == 0,
            "Mesh index offset {} is not aligned to index element size {}",
            gpu_mesh_record->allocation.index_offset,
            index_element_size);
        MIZU_ASSERT(
            gpu_mesh_record->allocation.vertex_offset % sizeof(MeshAssetVertex) == 0,
            "Mesh vertex offset {} is not aligned to MeshAssetVertex size {}",
            gpu_mesh_record->allocation.vertex_offset,
            sizeof(MeshAssetVertex));

        slot.drawable_info.gpu_mesh_draw = GpuMeshDrawPayload{
            .vertex_count = static_cast<uint32_t>(gpu_mesh_record->payload.vertex_count),
            .index_count = static_cast<uint32_t>(gpu_mesh_record->payload.index_count),
            .first_vertex = static_cast<uint32_t>(gpu_mesh_record->allocation.vertex_offset / sizeof(MeshAssetVertex)),
            .first_index = static_cast<uint32_t>(gpu_mesh_record->allocation.index_offset / index_element_size),
        };
        slot.drawable_info.material_buffer_offset = *material_buffer_offset;

        slot.drawable = true;
        slot.drawable_slot_index = allocate_drawable_slot(slot.drawable_info);
    }

    return slot.drawable;
}

bool SceneSystem::is_mesh_resident(const MeshAssetHandle& handle) const
{
    if (!handle.is_valid())
        return false;

    return m_mesh_residency_system.get_status(handle) == ResidencyStatus::GpuResident;
}

bool SceneSystem::is_material_resident(const MaterialAssetHandle& handle) const
{
    if (!handle.is_valid())
        return false;

    return m_material_residency_system.get_status(handle) == ResidencyStatus::GpuResident;
}

size_t SceneSystem::allocate_drawable_slot(SceneDrawableInfo info)
{
    const size_t index = m_drawable_slots.size();
    m_drawable_slots.push_back(std::move(info));

    return index;
}

void SceneSystem::free_drawable_slot(size_t index)
{
    MIZU_ASSERT(index < m_drawable_slots.size(), "Drawable index {} is out of range", index);

    const size_t last_index = m_drawable_slots.size() - 1;
    if (index != last_index)
    {
        const SceneDrawableInfo moved = m_drawable_slots[last_index];
        m_drawable_slots[index] = moved;

        const uint64_t moved_handle_id = moved.static_mesh_handle.get_internal_id();
        MIZU_ASSERT(moved_handle_id < m_slots.size(), "Moved drawable handle id {} out of range", moved_handle_id);

        RenderableSlot& moved_slot = m_slots[moved_handle_id];
        MIZU_ASSERT(moved_slot.occupied, "Moved drawable slot must be occupied");
        moved_slot.drawable_slot_index = index;
    }

    const auto diff_last = static_cast<std::ptrdiff_t>(last_index);
    m_drawable_slots.erase(
        std::next(m_drawable_slots.begin(), diff_last), std::next(m_drawable_slots.begin(), diff_last + 1));
}

uint32_t SceneSystem::allocate_transform_slot(const TransformHandle& handle)
{
    if (m_free_transform_slots.empty())
    {
        MIZU_ASSERT(false, "Could not allocate new transform slot");
        return INVALID_SLOT_U32;
    }

    const uint32_t slot = m_free_transform_slots.top();
    m_free_transform_slots.pop();

    m_transform_slot_indices[handle.get_internal_id()] = slot;

    return slot;
}

void SceneSystem::free_transform_slot(uint32_t slot)
{
    m_free_transform_slots.push(slot);
}

void SceneSystem::rend_on_update(TransformHandle handle, const TransformDynamicState& ds)
{
    const uint32_t slot = m_transform_slot_indices[handle.get_internal_id()];

    // If it's not a registered transform (has valid slot index) ignore
    if (slot == INVALID_SLOT_U32)
        return;

    // TODO: Should probably check if the transform ds has changed, though the state manager works with the assumption
    // that we only send updates through it if a dynamic state has changed.

    m_pending_transform_updates.push_back({
        .new_transform = build_transform_info(ds),
        .dst_slot = slot,
    });
}

TransformInfo SceneSystem::build_transform_info(const TransformDynamicState& ds)
{
    glm::mat4 transform{1.0f};
    transform = glm::translate(transform, ds.translation);
    transform = glm::rotate(transform, ds.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    transform = glm::rotate(transform, ds.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::rotate(transform, ds.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::scale(transform, ds.scale);

    return {
        .transform = transform,
        .normal_matrix = glm::transpose(glm::inverse(transform)),
    };
}

void SceneSystem::link_mesh_dependency(const MeshAssetHandle& handle, DependencyChain& chain, size_t slot_idx)
{
    auto it = m_mesh_dependency_head_map.find(handle);

    if (it == m_mesh_dependency_head_map.end())
    {
        m_mesh_dependency_head_map[handle] = slot_idx;
    }
    else
    {
        RenderableSlot& head_dependency_slot = m_slots[it->second];

        head_dependency_slot.mesh_dependency.prev = slot_idx;
        chain.next = it->second;

        it->second = slot_idx;
    }
}

void SceneSystem::link_material_dependency(const MaterialAssetHandle& handle, DependencyChain& chain, size_t slot_idx)
{
    auto it = m_material_dependency_head_map.find(handle);

    if (it == m_material_dependency_head_map.end())
    {
        m_material_dependency_head_map[handle] = slot_idx;
    }
    else
    {
        RenderableSlot& head_dependency_slot = m_slots[it->second];

        head_dependency_slot.material_dependency.prev = slot_idx;
        chain.next = it->second;

        it->second = slot_idx;
    }
}

void SceneSystem::unlink_mesh_dependency(const MeshAssetHandle& handle, size_t slot_idx)
{
    auto it = m_mesh_dependency_head_map.find(handle);
    if (it == m_mesh_dependency_head_map.end())
        return;

    size_t index = it->second;

    while (index != INVALID_SLOT)
    {
        RenderableSlot& slot = m_slots[index];

        if (index == slot_idx)
        {
            if (index == it->second)
            {
                m_mesh_dependency_head_map[handle] = slot.mesh_dependency.next;
            }
            else
            {
                RenderableSlot& prev_slot = m_slots[slot.mesh_dependency.prev];
                prev_slot.mesh_dependency.next = slot.mesh_dependency.next;

                if (slot.mesh_dependency.next != INVALID_SLOT)
                {
                    RenderableSlot& next_slot = m_slots[slot.mesh_dependency.next];
                    next_slot.mesh_dependency.prev = slot.mesh_dependency.prev;
                }
            }

            break;
        }

        index = slot.mesh_dependency.next;
    }
}

void SceneSystem::unlink_material_dependency(const MaterialAssetHandle& handle, size_t slot_idx)
{
    auto it = m_material_dependency_head_map.find(handle);
    if (it == m_material_dependency_head_map.end())
        return;

    size_t index = it->second;

    while (index != INVALID_SLOT)
    {
        RenderableSlot& slot = m_slots[index];

        if (index == slot_idx)
        {
            if (index == it->second)
            {
                m_material_dependency_head_map[handle] = slot.material_dependency.next;
            }
            else
            {
                RenderableSlot& prev_slot = m_slots[slot.material_dependency.prev];
                prev_slot.material_dependency.next = slot.material_dependency.next;

                if (slot.material_dependency.next != INVALID_SLOT)
                {
                    RenderableSlot& next_slot = m_slots[slot.material_dependency.next];
                    next_slot.material_dependency.prev = slot.material_dependency.prev;
                }
            }

            break;
        }

        index = slot.material_dependency.next;
    }
}

} // namespace Mizu