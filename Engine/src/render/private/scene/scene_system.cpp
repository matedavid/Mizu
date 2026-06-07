#include "scene/scene_system.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render.pipeline/material_shaders.h"
#include "render/utils/image_utils.h"
#include "resources/asset_load_system.h"
#include "resources/gpu_pools.h"
#include "resources/residency_system.h"

namespace Mizu
{

// TODO: TEMPORAL TEMPORAL TEMPORAL :)
SceneSystem* g_scene_system;

SceneSystem::SceneSystem(
    MeshResidencySystem& mesh_residency_system,
    MaterialResidencySystem& material_residency_system,
    GpuTexturePool& gpu_texture_pool,
    AssetLoadSystem& asset_load_system)
    : m_mesh_residency_system(mesh_residency_system)
    , m_material_residency_system(material_residency_system)
    , m_gpu_texture_pool(gpu_texture_pool)
    , m_asset_load_system(asset_load_system)
{
    // TODO: TEMPORAL TEMPORAL TEMPORAL :)
    g_scene_system = this;

    // TODO: TEMPORAL - remove when bindless material buffer is implemented
    ImageDescription default_desc{};
    default_desc.width = 1;
    default_desc.height = 1;
    default_desc.type = ImageType::Image2D;
    default_desc.format = ImageFormat::R8G8B8A8_SRGB;
    default_desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    default_desc.name = "SceneSystem_DefaultWhite";

    uint8_t white_data[] = {255, 255, 255, 255};
    m_default_white_texture = ImageUtils::create_texture2d(default_desc, white_data);
}

void SceneSystem::update(const ResourceEventStream& stream)
{
    MIZU_PROFILE_SCOPED;

    consume_renderable_events(stream);
    consume_mesh_residency_events(stream);
    consume_material_residency_events(stream);
}

void SceneSystem::consume_renderable_events(const ResourceEventStream& stream)
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
            handle_renderable_destroy_event(event);
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
            },
        .mesh_resident = is_mesh_resident(event.mesh_handle),
        .material_resident = is_material_resident(event.material_handle),
    };

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

void SceneSystem::handle_renderable_destroy_event(const RenderableEvent& event)
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
        const std::optional<uint32_t> material_buffer_slot =
            m_material_residency_system.get_material_buffer_slot(slot.drawable_info.material_handle);

        if (!gpu_mesh_record.has_value() || !material_buffer_slot.has_value())
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
        slot.drawable_info.material_buffer_slot = *material_buffer_slot;

        // TODO: TEMPORAL - create Material with baked descriptor sets instead of using
        // the bindless material buffer. Remove when shaders support bindless texture heap.
        {
            const std::optional<MaterialAssetRecord> material_record =
                m_asset_load_system.get_material_record(slot.drawable_info.material_handle);
            if (!material_record.has_value())
            {
                MIZU_LOG_ERROR(
                    "Failed to get material record for handle {} while transitioning to drawable",
                    slot.drawable_info.material_handle.get_id());
                return false;
            }

            const ShaderInstance& fs_instance = PBROpaqueShaderFS{}.get_instance();
            MaterialShaderInstance mat_shader{};
            mat_shader.virtual_path = fs_instance.virtual_path;
            mat_shader.entry_point = fs_instance.entry_point;

            auto material = std::make_shared<Material>(mat_shader);

            static constexpr std::array texture_names = {
                "albedo",
                "metallic",
                "roughness",
                "ambientOcclusion",
            };

            for (size_t i = 0; i < texture_names.size(); ++i)
            {
                std::shared_ptr<ImageResource> image;
                if (i < material_record->texture_handles.size())
                {
                    image = m_gpu_texture_pool.get_image(material_record->texture_handles[i]);
                }

                if (image == nullptr)
                    image = m_default_white_texture;

                // TODO: TEMPORAL - texture names must match DevAssetLoader order and shader bindings
                material->set_texture_srv(std::string(texture_names[i]), std::move(image));
            }

            // TODO: TEMPORAL - bake allocates persistent descriptor sets, bypassing bindless heap
            if (!material->bake())
            {
                MIZU_LOG_ERROR("Failed to bake material for handle {}", slot.drawable_info.material_handle.get_id());
                return false;
            }

            slot.drawable_info.material = std::move(material);
        }

        slot.drawable = true;
        slot.drawable_slot_index = allocate_drawable_slot(slot.drawable_info);
    }

    return slot.drawable;
}

bool SceneSystem::is_mesh_resident(const MeshAssetHandle& handle) const
{
    if (!handle.is_valid())
        return false;

    return m_mesh_residency_system.get_status(handle) == ResidencyStatus2::GpuResident;
}

bool SceneSystem::is_material_resident(const MaterialAssetHandle& handle) const
{
    if (!handle.is_valid())
        return false;

    return m_material_residency_system.get_status(handle) == ResidencyStatus2::GpuResident;
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