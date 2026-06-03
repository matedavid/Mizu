#include "resources/residency_system.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "render_core/rhi/buffer_resource.h"

#include "resources/cpu_loading_pool.h"
#include "resources/gpu_pools.h"

namespace Mizu
{

//
// ResidencySystemBase
//

#define RecordCpp ResidencySystemBase<AssetHandleType, RecordPayload>::Record
#define ShardCpp ResidencySystemBase<AssetHandleType, RecordPayload>::Shard

template <typename AssetHandleType, typename RecordPayload>
ResidencyStatus2 ResidencySystemBase<AssetHandleType, RecordPayload>::get_status(const AssetHandleType& handle) const
{
    const Record* record = get_record(handle);
    if (record == nullptr)
        return ResidencyStatus2::Unloaded;

    return record->status.load(std::memory_order_acquire);
}

template <typename AssetHandleType, typename RecordPayload>
bool ResidencySystemBase<AssetHandleType, RecordPayload>::increment_reference_count(const AssetHandleType& handle)
{
    Record* record = get_or_create_record(handle);
    if (record == nullptr)
        return false;

    record->references.fetch_add(1, std::memory_order_acq_rel);

    return true;
}

template <typename AssetHandleType, typename RecordPayload>
bool ResidencySystemBase<AssetHandleType, RecordPayload>::decrement_reference_count(const AssetHandleType& handle)
{
    Record* record = get_record(handle);
    if (record == nullptr)
        return false;

    // Can't decrement a record with 0 references
    if (record->references.load(std::memory_order_acquire) == 0)
        return false;

    record->references.fetch_sub(1, std::memory_order_acq_rel);

    return true;
}

template <typename AssetHandleType, typename RecordPayload>
bool ResidencySystemBase<AssetHandleType, RecordPayload>::transition_status(
    const AssetHandleType& handle,
    ResidencyStatus2 expected,
    ResidencyStatus2 desired)
{
    Record* record = get_record(handle);
    if (record == nullptr)
        return false;

    ResidencyStatus2 current = expected;
    while (!record->status.compare_exchange_weak(current, desired, std::memory_order_acq_rel))
    {
        if (current != expected)
            return false;

        current = expected;
    }

    return true;
}

template <typename AssetHandleType, typename RecordPayload>
RecordCpp* ResidencySystemBase<AssetHandleType, RecordPayload>::get_record(const AssetHandleType& handle)
{
    Shard& shard = get_shard(handle);

    std::lock_guard lock(shard.mutex);

    auto it = shard.records.find(handle.get_id());
    if (it == shard.records.end())
        return nullptr;

    return &it->second;
}

template <typename AssetHandleType, typename RecordPayload>
const RecordCpp* ResidencySystemBase<AssetHandleType, RecordPayload>::get_record(const AssetHandleType& handle) const
{
    const Shard& shard = get_shard(handle);

    std::lock_guard lock(shard.mutex);

    const auto it = shard.records.find(handle.get_id());
    if (it == shard.records.end())
        return nullptr;

    return &it->second;
}

template <typename AssetHandleType, typename RecordPayload>
RecordCpp* ResidencySystemBase<AssetHandleType, RecordPayload>::get_or_create_record(const AssetHandleType& handle)
{
    Shard& shard = get_shard(handle);

    std::lock_guard lock(shard.mutex);

    const auto it = shard.records.find(handle.get_id());
    if (it != shard.records.end())
        return &it->second;

    auto [insert_it, inserted] = shard.records.try_emplace(handle.get_id());
    if (!inserted)
        return nullptr;

    Record& record = insert_it->second;
    record.handle = handle;
    record.status.store(ResidencyStatus2::Unloaded, std::memory_order_release);
    record.references.store(0, std::memory_order_release);

    return &record;
}

template <typename AssetHandleType, typename RecordPayload>
ShardCpp& ResidencySystemBase<AssetHandleType, RecordPayload>::get_shard(const AssetHandleType& handle)
{
    const size_t shard_idx = handle.get_id() % NUM_SHARDS;
    return m_shards[shard_idx];
}

template <typename AssetHandleType, typename RecordPayload>
const ShardCpp& ResidencySystemBase<AssetHandleType, RecordPayload>::get_shard(const AssetHandleType& handle) const
{
    const size_t shard_idx = handle.get_id() % NUM_SHARDS;
    return m_shards[shard_idx];
}

#undef RecordCpp
#undef ShardCpp

//
// MeshResidencySystem
//

MeshResidencySystem::MeshResidencySystem(AssetLoadSystem& load_system, StreamingMeshRequestQueue& request_queue)
    : m_load_system(load_system)
    , m_request_queue(request_queue)
{
}

void MeshResidencySystem::update()
{
    MIZU_PROFILE_SCOPED;

    consume_requests();
    track_evictions();
}

void MeshResidencySystem::consume_requests()
{
    MeshStreamingRequest request;
    while (m_request_queue.pop(request))
    {
        switch (request.type)
        {
        case StreamingRequestType::Load:
            request_load(request);
            break;
        case StreamingRequestType::Evict:
            request_eviction(request);
            break;
        }
    }
}

void MeshResidencySystem::track_evictions() {}

void MeshResidencySystem::request_load(const MeshStreamingRequest& request)
{
    MIZU_ASSERT(request.type == StreamingRequestType::Load, "Invalid StreamingRequestType");

    if (!increment_reference_count(request.mesh_handle))
    {
        MIZU_ASSERT(false, "Failed to increment reference count for mesh handle: {}", request.mesh_handle.get_id());
        return;
    }

    const ResidencyStatus2 status = get_status(request.mesh_handle);

    // Already loaded, just increment the reference count
    if (status == ResidencyStatus2::Loading || status == ResidencyStatus2::GpuResident)
        return;

    MIZU_ASSERT(status == ResidencyStatus2::Unloaded, "Just in case we add a new ResidencyStatus2 value");

    if (!transition_status(request.mesh_handle, ResidencyStatus2::Unloaded, ResidencyStatus2::Loading))
    {
        MIZU_LOG_ERROR("Failed to transition mesh handle {} to Loading status", request.mesh_handle.get_id());
        return;
    }

    m_load_system.request_mesh_load(
        request.mesh_handle,
        [this](const MeshAssetHandle& handle, const CpuAllocationHandle& allocation_handle) {
            cpu_load_finished(handle, allocation_handle);
        },
        [this](const MeshAssetHandle& handle, const GpuMeshAllocationHandle& allocation_handle) {
            gpu_load_finished(handle, allocation_handle);
        });
}

void MeshResidencySystem::request_eviction(const MeshStreamingRequest& request)
{
    MIZU_ASSERT(request.type == StreamingRequestType::Evict, "Invalid StreamingRequestType");

    if (!decrement_reference_count(request.mesh_handle))
    {
        MIZU_ASSERT(false, "Failed to decrement reference count for mesh handle: {}", request.mesh_handle.get_id());
        return;
    }

    // Don't really need to do anything, eviction tracking will pick up handles with 0 references
    // TODO: Or should we keep an eviction list, so that we only iterate over that list instead of the entire residency
    // table?
}

void MeshResidencySystem::cpu_load_finished(const MeshAssetHandle& handle, const CpuAllocationHandle& allocation_handle)
{
    (void)handle;
    (void)allocation_handle;
}

void MeshResidencySystem::gpu_load_finished(
    const MeshAssetHandle& handle,
    [[maybe_unused]] const GpuMeshAllocationHandle& allocation_handle)
{
    if (!transition_status(handle, ResidencyStatus2::Loading, ResidencyStatus2::GpuResident))
    {
        MIZU_LOG_ERROR("Failed to transition mesh handle {} to GpuResident status", handle.get_id());
    }

    Record* record = get_record(handle);
    MIZU_ASSERT(record != nullptr, "Record should exist for handle that just finished loading");

    record->payload = MeshResidencySystemPayload{
        .gpu_allocation = allocation_handle,
    };
}

//
// TextureResidencySystem
//

TextureResidencySystem::TextureResidencySystem(
    AssetLoadSystem& load_system,
    StreamingTextureRequestQueue& request_queue,
    GpuTexturePool& gpu_texture_pool)
    : m_load_system(load_system)
    , m_request_queue(request_queue)
    , m_gpu_texture_pool(gpu_texture_pool)
{
    static constexpr uint32_t NUM_BINDLESS_TEXTURES = 1'000;

    const std::array bindless_texture_layout = {
        DescriptorItem::TextureSrv(0, BINDLESS_DESCRIPTOR_COUNT, ShaderType::Fragment),
    };

    const DescriptorSetLayoutHandle bindless_texture_layout_handle = g_render_device->create_descriptor_set_layout({
        .layout = bindless_texture_layout,
    });

    m_bindless_texture_descriptor_set = g_render_device->allocate_descriptor_set(
        bindless_texture_layout_handle, DescriptorSetAllocationType::Bindless, NUM_BINDLESS_TEXTURES);

    m_free_bindless_slots.resize(NUM_BINDLESS_TEXTURES);
    std::iota(m_free_bindless_slots.rbegin(), m_free_bindless_slots.rend(), 0);
}

void TextureResidencySystem::update()
{
    MIZU_PROFILE_SCOPED;

    consume_requests();
    track_evictions();
}

void TextureResidencySystem::request_dependency_load(const TextureAssetHandle& handle)
{
    request_load({
        .type = StreamingRequestType::Load,
        .texture_handle = handle,
    });
}

void TextureResidencySystem::request_dependency_evict(const TextureAssetHandle& handle)
{
    request_eviction({
        .type = StreamingRequestType::Evict,
        .texture_handle = handle,
    });
}

std::optional<uint32_t> TextureResidencySystem::get_bindless_descriptor_slot(const TextureAssetHandle& handle) const
{
    const Record* record = get_record(handle);
    if (record == nullptr)
        return std::nullopt;

    if (record->status.load(std::memory_order_relaxed) != ResidencyStatus2::GpuResident)
        return std::nullopt;

    if (record->payload.bindless_descriptor_slot == std::numeric_limits<uint32_t>::max())
        return std::nullopt;

    return record->payload.bindless_descriptor_slot;
}

void TextureResidencySystem::consume_requests()
{
    TextureStreamingRequest request;
    while (m_request_queue.pop(request))
    {
        switch (request.type)
        {
        case StreamingRequestType::Load:
            request_load(request);
            break;
        case StreamingRequestType::Evict:
            request_eviction(request);
            break;
        }
    }
}

void TextureResidencySystem::track_evictions() {}

void TextureResidencySystem::request_load(const TextureStreamingRequest& request)
{
    MIZU_ASSERT(request.type == StreamingRequestType::Load, "Invalid StreamingRequestType");

    if (!increment_reference_count(request.texture_handle))
    {
        MIZU_ASSERT(
            false, "Failed to increment reference count for texture handle: {}", request.texture_handle.get_id());
        return;
    }

    const ResidencyStatus2 status = get_status(request.texture_handle);
    if (status == ResidencyStatus2::Loading || status == ResidencyStatus2::GpuResident)
        return;

    MIZU_ASSERT(status == ResidencyStatus2::Unloaded, "Unexpected texture residency status");

    if (!transition_status(request.texture_handle, ResidencyStatus2::Unloaded, ResidencyStatus2::Loading))
    {
        MIZU_LOG_ERROR("Failed to transition texture handle {} to Loading status", request.texture_handle.get_id());
        return;
    }

    m_load_system.request_texture_load(
        request.texture_handle,
        [this](const TextureAssetHandle& handle, const CpuAllocationHandle& allocation_handle) {
            cpu_load_finished(handle, allocation_handle);
        },
        [this](const TextureAssetHandle& handle, const GpuTextureAllocationHandle& allocation_handle) {
            gpu_load_finished(handle, allocation_handle);
        });
}

void TextureResidencySystem::request_eviction(const TextureStreamingRequest& request)
{
    MIZU_ASSERT(request.type == StreamingRequestType::Evict, "Invalid StreamingRequestType");

    if (!decrement_reference_count(request.texture_handle))
    {
        MIZU_ASSERT(
            false, "Failed to decrement reference count for texture handle: {}", request.texture_handle.get_id());
        return;
    }

    // Don't really need to do anything, eviction tracking will pick up handles with 0 references
    // TODO: Or should we keep an eviction list, so that we only iterate over that list instead of the entire residency
    // table?
}

void TextureResidencySystem::cpu_load_finished(
    const TextureAssetHandle& handle,
    const CpuAllocationHandle& allocation_handle)
{
    (void)handle;
    (void)allocation_handle;
}

void TextureResidencySystem::gpu_load_finished(
    const TextureAssetHandle& handle,
    [[maybe_unused]] const GpuTextureAllocationHandle& allocation_handle)
{
    if (!transition_status(handle, ResidencyStatus2::Loading, ResidencyStatus2::GpuResident))
    {
        MIZU_LOG_ERROR("Failed to transition mesh handle {} to GpuResident status", handle.get_id());
        return;
    }

    const std::shared_ptr<ImageResource> image_resource = m_gpu_texture_pool.get_image(allocation_handle);
    if (image_resource == nullptr)
    {
        MIZU_LOG_ERROR("Failed to get ImageResource for GPU texture allocation of handle: {}", handle.get_id());
        return;
    }

    const std::optional<uint32_t> bindless_slot = allocate_bindless_descriptor_slot();
    if (!bindless_slot.has_value())
    {
        MIZU_LOG_ERROR("Failed to allocate bindless descriptor slot for texture handle: {}", handle.get_id());
        return;
    }

    const std::array descriptor_writes = {
        WriteDescriptor::TextureSrv(0, ImageResourceView::create(image_resource)),
    };
    m_bindless_texture_descriptor_set->update(descriptor_writes, *bindless_slot);

    Record* record = get_record(handle);
    MIZU_ASSERT(record != nullptr, "Record should exist for handle that just finished loading");

    record->payload = TextureResidencySystemPayload{
        .gpu_allocation = allocation_handle,
        .bindless_descriptor_slot = *bindless_slot,
    };
}

std::optional<uint32_t> TextureResidencySystem::allocate_bindless_descriptor_slot()
{
    if (m_free_bindless_slots.empty())
        return std::nullopt;

    const uint32_t slot = m_free_bindless_slots.back();
    m_free_bindless_slots.pop_back();

    return slot;
}

void TextureResidencySystem::free_bindless_descriptor_slot(uint32_t slot)
{
    m_free_bindless_slots.push_back(slot);
}

//
// MaterialResidencySystem
//

MaterialResidencySystem::MaterialResidencySystem(
    AssetLoadSystem& load_system,
    StreamingMaterialRequestQueue& request_queue,
    TextureResidencySystem& texture_residency_system)
    : m_load_system(load_system)
    , m_request_queue(request_queue)
    , m_texture_residency_system(texture_residency_system)
{
    m_pending_records.reserve(StaticMeshConfig::MaxNumHandles);

    m_free_material_buffer_slots.resize(StaticMeshConfig::MaxNumHandles);
    std::iota(m_free_material_buffer_slots.rbegin(), m_free_material_buffer_slots.rend(), 0);

    BufferDescription material_buffer_desc{};
    material_buffer_desc.size = StaticMeshConfig::MaxNumHandles * sizeof(uint32_t) * MAX_TEXTURES_PER_MATERIAL;
    material_buffer_desc.stride = sizeof(uint32_t);
    material_buffer_desc.usage = BufferUsageBits::ShaderResource | BufferUsageBits::HostVisible;
    material_buffer_desc.name = "MaterialResidencySystem_MaterialBuffer";

    m_material_buffer = g_render_device->create_buffer(material_buffer_desc);
}

void MaterialResidencySystem::update()
{
    MIZU_PROFILE_SCOPED;

    consume_requests();
    refresh_pending_materials();
}

void MaterialResidencySystem::consume_requests()
{
    MaterialStreamingRequest request;
    while (m_request_queue.pop(request))
    {
        switch (request.type)
        {
        case StreamingRequestType::Load:
            request_load(request);
            break;
        case StreamingRequestType::Evict:
            request_eviction(request);
            break;
        }
    }
}

void MaterialResidencySystem::refresh_pending_materials()
{
    auto it = m_pending_records.begin();
    while (it != m_pending_records.end())
    {
        const MaterialAssetRecord& record = *it;

        if (material_dependencies_loaded(record))
        {
            material_load_finished(record);
            it = m_pending_records.erase(it);
        }
        else
        {
            it = std::next(it);
        }
    }
}

void MaterialResidencySystem::request_load(const MaterialStreamingRequest& request)
{
    MIZU_ASSERT(request.type == StreamingRequestType::Load, "Invalid StreamingRequestType");

    if (!increment_reference_count(request.material_handle))
    {
        MIZU_ASSERT(false, "Failed to increment reference count for mesh handle: {}", request.material_handle.get_id());
        return;
    }

    const ResidencyStatus2 status = get_status(request.material_handle);

    // Already loaded, just increment the reference count
    if (status == ResidencyStatus2::Loading || status == ResidencyStatus2::GpuResident)
        return;

    MIZU_ASSERT(status == ResidencyStatus2::Unloaded, "Just in case we add a new ResidencyStatus2 value");

    const std::optional<MaterialAssetRecord> material_record =
        m_load_system.get_material_record(request.material_handle);

    if (!material_record.has_value())
    {
        MIZU_LOG_ERROR("Failed to resolve material record for handle {}", request.material_handle.get_id());
        return;
    }

    if (!transition_status(request.material_handle, ResidencyStatus2::Unloaded, ResidencyStatus2::Loading))
    {
        MIZU_LOG_ERROR("Failed to transition material handle {} to Loading status", request.material_handle.get_id());
        return;
    }

    for (const TextureAssetHandle& texture_handle : material_record->texture_handles)
    {
        m_texture_residency_system.request_dependency_load(texture_handle);
    }

    m_pending_records.push_back(*material_record);
}

void MaterialResidencySystem::request_eviction(const MaterialStreamingRequest& request)
{
    MIZU_ASSERT(request.type == StreamingRequestType::Evict, "Invalid StreamingRequestType");

    if (!decrement_reference_count(request.material_handle))
    {
        MIZU_ASSERT(
            false, "Failed to decrement reference count for material handle: {}", request.material_handle.get_id());
        return;
    }

    const std::optional<MaterialAssetRecord> material_record =
        m_load_system.get_material_record(request.material_handle);

    if (!material_record.has_value())
    {
        MIZU_LOG_ERROR("Failed to resolve material record for handle {}", request.material_handle.get_id());
        return;
    }

    for (const TextureAssetHandle& texture_handle : material_record->texture_handles)
    {
        m_texture_residency_system.request_dependency_evict(texture_handle);
    }

    // Don't really need to do anything, eviction tracking will pick up handles with 0 references
    // TODO: Or should we keep an eviction list, so that we only iterate over that list instead of the entire residency
    // table?
}

bool MaterialResidencySystem::material_dependencies_loaded(const MaterialAssetRecord& record) const
{
    for (const TextureAssetHandle& texture_handle : record.texture_handles)
    {
        if (m_texture_residency_system.get_status(texture_handle) != ResidencyStatus2::GpuResident)
            return false;
    }

    return true;
}

void MaterialResidencySystem::material_load_finished(const MaterialAssetRecord& record)
{
    if (!transition_status(record.handle, ResidencyStatus2::Loading, ResidencyStatus2::GpuResident))
    {
        MIZU_LOG_ERROR("Failed to transition material handle {} to GpuResident status", record.handle.get_id());
        return;
    }

    // Allocate slot in material buffer, and copy texture indices into bindless array (from m_texture_residency_system)
    // TODO:

    const std::optional<uint32_t> material_buffer_slot = allocate_material_buffer_slot();
    if (!material_buffer_slot.has_value())
    {
        MIZU_LOG_ERROR("Failed to allocate material buffer slot for material handle: {}", record.handle.get_id());
        return;
    }

    // TODO: Dynamic allocation bad
    std::vector<uint32_t> texture_bindless_slots;
    texture_bindless_slots.reserve(record.texture_handles.size());

    for (const TextureAssetHandle& texture_handle : record.texture_handles)
    {
        const std::optional<uint32_t> bindless_slot =
            m_texture_residency_system.get_bindless_descriptor_slot(texture_handle);

        if (!bindless_slot.has_value())
        {
            MIZU_LOG_ERROR(
                "Failed to get bindless descriptor slot for texture handle {} while loading material handle {}",
                texture_handle.get_id(),
                record.handle.get_id());

            return;
        }

        texture_bindless_slots.push_back(*bindless_slot);
    }

    uint8_t* mapped_data = m_material_buffer->get_mapped_data();
    MIZU_ASSERT(mapped_data != nullptr, "Failed to map material buffer");

    const uint32_t stride = sizeof(uint32_t) * MAX_TEXTURES_PER_MATERIAL;
    uint8_t* mapped_data_slot = std::next(mapped_data, *material_buffer_slot * stride);

    memcpy(mapped_data_slot, texture_bindless_slots.data(), texture_bindless_slots.size() * sizeof(uint32_t));

    Record* residency_record = get_record(record.handle);
    MIZU_ASSERT(residency_record != nullptr, "Record should exist for handle that just finished loading");

    residency_record->payload = MaterialResidencySystemPayload{
        .material_buffer_slot = *material_buffer_slot,
    };
}

std::optional<uint32_t> MaterialResidencySystem::allocate_material_buffer_slot()
{
    if (m_free_material_buffer_slots.empty())
        return std::nullopt;

    const uint32_t slot = m_free_material_buffer_slots.back();
    m_free_material_buffer_slots.pop_back();

    return slot;
}

void MaterialResidencySystem::free_material_buffer_slot(uint32_t slot)
{
    m_free_material_buffer_slots.push_back(slot);
}

} // namespace Mizu
