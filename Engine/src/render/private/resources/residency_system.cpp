#include "resources/residency_system.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

//
// ResidencySystemBase
//

#define RecordCpp ResidencySystemBase<AssetHandleType>::Record
#define ShardCpp ResidencySystemBase<AssetHandleType>::Shard

template <typename AssetHandleType>
ResidencyStatus2 ResidencySystemBase<AssetHandleType>::get_status(const AssetHandleType& handle) const
{
    const Record* record = get_record(handle);
    if (record == nullptr)
        return ResidencyStatus2::Unloaded;

    return record->status.load(std::memory_order_acquire);
}

template <typename AssetHandleType>
bool ResidencySystemBase<AssetHandleType>::increment_reference_count(const AssetHandleType& handle)
{
    Record* record = get_or_create_record(handle);
    if (record == nullptr)
        return false;

    record->references.fetch_add(1, std::memory_order_acq_rel);

    return true;
}

template <typename AssetHandleType>
bool ResidencySystemBase<AssetHandleType>::decrement_reference_count(const AssetHandleType& handle)
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

template <typename AssetHandleType>
bool ResidencySystemBase<AssetHandleType>::transition_status(
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

template <typename AssetHandleType>
RecordCpp* ResidencySystemBase<AssetHandleType>::get_record(const AssetHandleType& handle)
{
    Shard& shard = get_shard(handle);

    std::lock_guard lock(shard.mutex);

    auto it = shard.records.find(handle.get_id());
    if (it == shard.records.end())
        return nullptr;

    return &it->second;
}

template <typename AssetHandleType>
const RecordCpp* ResidencySystemBase<AssetHandleType>::get_record(const AssetHandleType& handle) const
{
    const Shard& shard = get_shard(handle);

    std::lock_guard lock(shard.mutex);

    const auto it = shard.records.find(handle.get_id());
    if (it == shard.records.end())
        return nullptr;

    return &it->second;
}

template <typename AssetHandleType>
RecordCpp* ResidencySystemBase<AssetHandleType>::get_or_create_record(const AssetHandleType& handle)
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

template <typename AssetHandleType>
ShardCpp& ResidencySystemBase<AssetHandleType>::get_shard(const AssetHandleType& handle)
{
    const size_t shard_idx = handle.get_id() % NUM_SHARDS;
    return m_shards[shard_idx];
}

template <typename AssetHandleType>
const ShardCpp& ResidencySystemBase<AssetHandleType>::get_shard(const AssetHandleType& handle) const
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
    const GpuMeshAllocationHandle& allocation_handle)
{
    (void)handle;
    (void)allocation_handle;
}

//
// TextureResidencySystem
//

TextureResidencySystem::TextureResidencySystem(
    AssetLoadSystem& load_system,
    StreamingTextureRequestQueue& request_queue)
    : m_load_system(load_system)
    , m_request_queue(request_queue)
{
}

void TextureResidencySystem::update()
{
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
    const GpuTextureAllocationHandle& allocation_handle)
{
    (void)handle;
    (void)allocation_handle;
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
}

void MaterialResidencySystem::update()
{
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
}

} // namespace Mizu
