#include "resources/residency_manager.h"

#include <algorithm>
#include <array>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/runtime.h"

namespace Mizu
{

//
// ResidencyTable
//

#define ResidencyEntryIndexCpp typename ResidencyTable<AssetHandleType, GpuPoolType>::ResidencyEntryIndex

template <typename AssetHandleType, typename GpuPoolType>
ResidencyTable<AssetHandleType, GpuPoolType>::ResidencyTable(GpuPoolType& gpu_pool) : m_gpu_pool(gpu_pool)
{
}

template <typename AssetHandleType, typename GpuPoolType>
ResidencyStatus ResidencyTable<AssetHandleType, GpuPoolType>::request_load(
    AssetHandleType handle,
    bool& out_enqueue_job)
{
    out_enqueue_job = false;

    const std::optional<ResidencyEntryIndex> entry_index = get_entry_index(handle);
    if (entry_index.has_value())
    {
        ResidencyEntry& entry = m_entries[static_cast<uint64_t>(entry_index->index)];

        if (entry.generation != entry_index->generation)
        {
            MIZU_ASSERT(false, "ResidencyEntryIndex and ResidencyEntry generations do not match");
            return ResidencyStatus::Unloaded;
        }

        ResidencyStatus current_status = entry.status.load(std::memory_order_acquire);
        if (current_status == ResidencyStatus::Loaded || current_status == ResidencyStatus::Loading)
            return current_status;

        if (current_status == ResidencyStatus::Evicting
            && entry.status.compare_exchange_strong(
                current_status, ResidencyStatus::Loaded, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return ResidencyStatus::Loaded;
        }
    }

    out_enqueue_job = true;
    return ResidencyStatus::Loading;
}

template <typename AssetHandleType, typename GpuPoolType>
ResidencyStatus ResidencyTable<AssetHandleType, GpuPoolType>::get_status(AssetHandleType handle) const
{
    const std::optional<ResidencyEntryIndex> entry_index = get_entry_index(handle);
    if (!entry_index.has_value())
        return ResidencyStatus::Unloaded;

    const ResidencyEntry& entry = m_entries[static_cast<uint64_t>(entry_index->index)];

    if (entry.generation != entry_index->generation)
    {
        MIZU_LOG_WARNING("ResidencyEntryIndex and ResidencyEntry generations do not match");
        return ResidencyStatus::Unloaded;
    }

    return entry.status.load(std::memory_order_acquire);
}

template <typename AssetHandleType, typename GpuPoolType>
std::optional<ResidencyEntryIndexCpp> ResidencyTable<AssetHandleType, GpuPoolType>::get_entry_index(
    const AssetHandleType& handle) const
{
    const uint64_t handle_id = handle.get_id();
    const size_t shard_idx = handle_id % NumShards;

    const Shard& shard = m_slot_shards[shard_idx];

    std::shared_lock lock{shard.mutex};

    const auto it = shard.handle_to_entry_slot_map.find(handle);
    if (it != shard.handle_to_entry_slot_map.end())
        return it->second;

    return std::nullopt;
}

#undef ResidencyEntryIndexCpp

//
// ResidencyManager
//

static constexpr size_t MaxLoadJobs = 8;
static constexpr size_t MaxAssetsPerLoadJob = 16;
static constexpr size_t MinAssetsPerLoadJob = 2;

ResidencyManager::ResidencyManager(
    IAssetLoader& asset_loader,
    CpuLoadingPool& cpu_loading_pool,
    GpuMeshPool& mesh_pool,
    GpuTexturePool& texture_pool)
    : m_asset_loader(asset_loader)
    , m_cpu_loading_pool(cpu_loading_pool)
    , m_mesh_residency_table(mesh_pool)
    , m_texture_residency_table(texture_pool)
{
    m_load_jobs_in_progress.resize(MaxLoadJobs * MaxAssetsPerLoadJob, LoadJobRecordPoolIndex{});

    for (size_t i = 0; i < MaxLoadJobs; ++i)
        available_job_in_progress_slots.push(i);
}

ResidencyStatus ResidencyManager::request_load(MeshAssetHandle handle)
{
    bool should_enqueue_load_job = false;
    const ResidencyStatus status = m_mesh_residency_table.request_load(handle, should_enqueue_load_job);

    if (status == ResidencyStatus::Loaded || status == ResidencyStatus::Unloaded)
        return status;

    if (status == ResidencyStatus::Loading && !should_enqueue_load_job)
        return status;

    if (should_enqueue_load_job)
    {
        LoadJobRecordPoolIndex index = m_load_job_record_pool.allocate();

        LoadJobRecord& record = m_load_job_record_pool.get(index);
        record.handle = handle;
        record.num_dependencies.store(0, std::memory_order_relaxed);
        record.waiting_jobs_head.store(WaitNodePoolIndex{});

        m_load_jobs_queue.push(index);
        m_num_load_jobs_in_queue.fetch_add(1, std::memory_order_relaxed);

        return ResidencyStatus::Loading;
    }

    return ResidencyStatus::Unloaded;
}

ResidencyStatus ResidencyManager::request_load(TextureAssetHandle handle)
{
    bool should_enqueue_load_job = false;
    const ResidencyStatus status = m_texture_residency_table.request_load(handle, should_enqueue_load_job);

    if (status == ResidencyStatus::Loaded || status == ResidencyStatus::Unloaded)
        return status;

    if (status == ResidencyStatus::Loading && !should_enqueue_load_job)
        return status;

    if (should_enqueue_load_job)
    {
        LoadJobRecordPoolIndex index = m_load_job_record_pool.allocate();

        LoadJobRecord& record = m_load_job_record_pool.get(index);
        record.handle = handle;
        record.num_dependencies.store(0, std::memory_order_relaxed);
        record.waiting_jobs_head.store(WaitNodePoolIndex{});

        m_load_jobs_queue.push(index);
        m_num_load_jobs_in_queue.fetch_add(1, std::memory_order_relaxed);

        return ResidencyStatus::Loading;
    }

    return ResidencyStatus::Unloaded;
}

ResidencyStatus ResidencyManager::get_status(MeshAssetHandle handle) const
{
    return m_mesh_residency_table.get_status(handle);
}

ResidencyStatus ResidencyManager::get_status(TextureAssetHandle handle) const
{
    return m_texture_residency_table.get_status(handle);
}

void ResidencyManager::dispatch_jobs()
{
    const auto ceil_div = [](size_t numerator, size_t denominator) -> size_t {
        return (numerator + denominator - 1) / denominator;
    };

    if (m_num_load_jobs_in_queue.load(std::memory_order_relaxed) == 0
        || m_info.load_jobs_in_progress.load(std::memory_order_acquire) >= MaxLoadJobs)
        return;

    const size_t max_num_jobs_to_dispatch = MaxLoadJobs - m_info.load_jobs_in_progress.load(std::memory_order_acquire);
    const size_t num_requested_loads = m_num_load_jobs_in_queue.load(std::memory_order_relaxed);

    const size_t num_assets_to_dispatch = std::min(num_requested_loads, max_num_jobs_to_dispatch * MaxAssetsPerLoadJob);

    const size_t min_num_jobs_for_max_batch_size = ceil_div(num_assets_to_dispatch, MaxAssetsPerLoadJob);
    const size_t max_num_jobs_for_min_batch_size =
        num_assets_to_dispatch < MinAssetsPerLoadJob ? 1 : num_assets_to_dispatch / MinAssetsPerLoadJob;

    const size_t num_jobs_to_dispatch =
        std::max(min_num_jobs_for_max_batch_size, std::min(max_num_jobs_to_dispatch, max_num_jobs_for_min_batch_size));

    const size_t assets_per_job = num_assets_to_dispatch / num_jobs_to_dispatch;
    const size_t num_jobs_with_extra_asset = num_assets_to_dispatch % num_jobs_to_dispatch;

    std::array<size_t, MaxLoadJobs> num_assets_per_job{};
    std::fill(num_assets_per_job.begin(), num_assets_per_job.end(), assets_per_job);

    for (size_t job_index = 0; job_index < num_jobs_with_extra_asset; ++job_index)
        num_assets_per_job[job_index] += 1;

    for (size_t job_index = 0; job_index < num_jobs_to_dispatch; ++job_index)
    {
        const size_t num_assets_for_this_job = num_assets_per_job[job_index];

        const size_t job_in_progress_slot = [&]() {
            std::lock_guard lock{m_load_jobs_in_progress_mutex};

            const size_t slot = available_job_in_progress_slots.front();
            available_job_in_progress_slots.pop();

            return slot;
        }();

        const size_t jobs_in_progress_start = job_in_progress_slot * MaxAssetsPerLoadJob;

        for (size_t asset_index = 0; asset_index < num_assets_for_this_job; ++asset_index)
        {
            LoadJobRecordPoolIndex load_job_record_index{};
            if (!m_load_jobs_queue.pop(load_job_record_index))
            {
                MIZU_ASSERT(false, "Failed to pop from load jobs queue while dispatching jobs");
                return;
            }

            m_num_load_jobs_in_queue.fetch_sub(1, std::memory_order_relaxed);

            m_load_jobs_in_progress[jobs_in_progress_start + asset_index] = load_job_record_index;
        }

        m_info.load_jobs_in_progress.fetch_add(1, std::memory_order_release);
        g_job_system->schedule(&ResidencyManager::asset_load_job, this, jobs_in_progress_start, num_assets_for_this_job)
            .name("ResidencyManagerLoadJob")
            .submit();
    }
}

void ResidencyManager::asset_load_job(size_t jobs_in_progress_start, size_t num_assets)
{
    MIZU_PROFILE_SCOPED;

    for (size_t i = jobs_in_progress_start; i < jobs_in_progress_start + num_assets; ++i)
    {
        const LoadJobRecordPoolIndex load_job_record_index = m_load_jobs_in_progress[i];
        LoadJobRecord& record = m_load_job_record_pool.get(load_job_record_index);

        const bool loaded = std::visit([this](auto&& handle) { return load_asset(handle); }, record.handle);
        MIZU_ASSERT(loaded, "Failed to load asset in load job");

        process_load_job_completion(record);
    }

    {
        std::lock_guard lock{m_load_jobs_in_progress_mutex};
        available_job_in_progress_slots.push(jobs_in_progress_start / MaxAssetsPerLoadJob);
    }

    m_info.load_jobs_in_progress.fetch_sub(1, std::memory_order_release);
}

void ResidencyManager::process_load_job_completion(LoadJobRecord& record)
{
    WaitNodePoolIndex wait_node_index =
        record.waiting_jobs_head.exchange(LoadJobRecord::ClosedWaitingList, std::memory_order_acq_rel);

    WaitNodePoolIndex current_index = wait_node_index;
    while (current_index.is_valid())
    {
        const WaitNode& wait_node = m_wait_node_pool.get(current_index);

        LoadJobRecord& load_job = m_load_job_record_pool.get(wait_node.target_load_job_index);

        const uint32_t remaining_deps = load_job.num_dependencies.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining_deps == 0)
        {
            m_load_jobs_queue.push(wait_node.target_load_job_index);
        }

        current_index = wait_node.next;

        // Free the wait node after processing it
        m_wait_node_pool.free(wait_node.pool_index);
    }

    m_load_job_record_pool.free(record.pool_index);
}

bool ResidencyManager::load_asset(MeshAssetHandle handle)
{
    MIZU_ASSERT(handle.is_valid(), "Trying to load invalid MeshAssetHandle");

    const std::optional<CpuAllocationHandle> allocation = allocate_asset(handle);
    if (!allocation.has_value())
    {
        MIZU_LOG_ERROR("Failed to load mesh asset for handle '{}'", handle.get_id());
        return false;
    }

    // TODO: Add to pending streaming queue
    (void)allocation;

    return true;
}

bool ResidencyManager::load_asset(TextureAssetHandle handle)
{
    MIZU_ASSERT(handle.is_valid(), "Trying to load invalid TextureAssetHandle");

    const std::optional<CpuAllocationHandle> allocation = allocate_asset(handle);
    if (!allocation.has_value())
    {
        MIZU_LOG_ERROR("Failed to load mesh asset for handle '{}'", handle.get_id());
        return false;
    }

    // TODO: Add to pending streaming queue
    (void)allocation;

    return true;
}

std::optional<CpuAllocationHandle> ResidencyManager::allocate_asset(MeshAssetHandle handle)
{
    if (const std::optional<CpuAllocationHandle> cached_allocation = m_cpu_loading_pool.get_mesh(handle))
        return cached_allocation;

    const std::optional<MeshAssetRecord> asset_record = m_asset_loader.get_mesh_record(handle);
    if (!asset_record.has_value())
    {
        MIZU_LOG_ERROR("Failed to resolve mesh asset record for handle '{}'", handle.get_id());
        return std::nullopt;
    }

    const CpuLoadAcquireResult acquire_result =
        m_cpu_loading_pool.acquire_mesh(handle, asset_record->payload.size_bytes, asset_record->payload.alignment);

    if (acquire_result.status == CpuLoadAcquireStatus::CacheHit)
        return acquire_result.allocation;

    if (acquire_result.status == CpuLoadAcquireStatus::PendingLoad)
        return std::nullopt;

    if (acquire_result.status == CpuLoadAcquireStatus::Failed)
    {
        MIZU_LOG_ERROR("Failed to reserve mesh asset '{}' in CpuLoadingPool", handle.get_id());
        return std::nullopt;
    }

    if (!m_asset_loader.load_mesh_payload(handle, acquire_result.allocation.data))
    {
        m_cpu_loading_pool.abort_mesh(handle);
        MIZU_LOG_ERROR("Failed to load mesh payload for handle '{}'", handle.get_id());
        return std::nullopt;
    }

    m_cpu_loading_pool.commit_mesh(handle);
    return acquire_result.allocation;
}

std::optional<CpuAllocationHandle> ResidencyManager::allocate_asset(TextureAssetHandle handle)
{
    if (const std::optional<CpuAllocationHandle> cached_allocation = m_cpu_loading_pool.get_texture(handle))
        return cached_allocation;

    const std::optional<TextureAssetRecord> asset_record = m_asset_loader.get_texture_record(handle);
    if (!asset_record.has_value())
    {
        MIZU_LOG_ERROR("Failed to resolve texture asset record for handle '{}'", handle.get_id());
        return std::nullopt;
    }

    const CpuLoadAcquireResult acquire_result =
        m_cpu_loading_pool.acquire_texture(handle, asset_record->payload.size_bytes, asset_record->payload.alignment);

    if (acquire_result.status == CpuLoadAcquireStatus::CacheHit)
        return acquire_result.allocation;

    if (acquire_result.status == CpuLoadAcquireStatus::PendingLoad)
        return std::nullopt;

    if (acquire_result.status == CpuLoadAcquireStatus::Failed)
    {
        MIZU_LOG_ERROR("Failed to reserve texture asset '{}' in CpuLoadingPool", handle.get_id());
        return std::nullopt;
    }

    if (!m_asset_loader.load_texture_payload(handle, acquire_result.allocation.data))
    {
        m_cpu_loading_pool.abort_texture(handle);
        MIZU_LOG_ERROR("Failed to load texture payload for handle '{}'", handle.get_id());
        return std::nullopt;
    }

    m_cpu_loading_pool.commit_texture(handle);
    return acquire_result.allocation;
}

} // namespace Mizu
