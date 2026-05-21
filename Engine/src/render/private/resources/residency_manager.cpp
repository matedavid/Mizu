#include "resources/residency_manager.h"

#include <algorithm>
#include <array>

#include "asset/asset.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/runtime.h"

namespace Mizu
{

//
// ResidencyTable
//

static constexpr uint32_t EvictionThresholdFrames = 10;

#define ResidencyEntryCpp ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::ResidencyEntry

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::ResidencyTable(GpuPoolType& gpu_pool)
    : m_gpu_pool(gpu_pool)
{
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
ResidencyStatus ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::request_load(
    AssetHandleType handle,
    bool& out_enqueue_job)
{
    out_enqueue_job = false;

    ResidencyEntry& entry = get_or_create_residency_entry(handle);
    entry.references.fetch_add(1, std::memory_order_acq_rel);
    entry.num_frames_in_eviction = 0;

    ResidencyStatus current_status = entry.status.load(std::memory_order_relaxed);
    while (true)
    {
        out_enqueue_job = false;

        ResidencyStatus expected_status{};
        switch (current_status)
        {
        case ResidencyStatus::Unloaded:
            out_enqueue_job = true; // only enqueue load job when going from unloaded to loading.
            expected_status = ResidencyStatus::Loading;
            break;
        case ResidencyStatus::Loading:
            expected_status = ResidencyStatus::Loading;
            break;
        case ResidencyStatus::Loaded:
            expected_status = ResidencyStatus::Loaded;
            break;
        case ResidencyStatus::Evicting:
            expected_status = ResidencyStatus::Loaded;
            break;
        }

        if (entry.status.compare_exchange_weak(
                current_status, expected_status, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return expected_status;
        }
    }

    MIZU_UNREACHABLE("Failed to satisfy load request");

    out_enqueue_job = false;
    return ResidencyStatus::Unloaded;
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
bool ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::request_unload(AssetHandleType handle)
{
    ResidencyEntry& entry = get_residency_entry(handle);
    const uint32_t new_references = entry.references.fetch_sub(1, std::memory_order_acq_rel) - 1;

    if (new_references > 0)
        return true;

    ResidencyStatus current_status = entry.status.load(std::memory_order_relaxed);
    while (true)
    {
        ResidencyStatus expected_status{};
        switch (current_status)
        {
        case ResidencyStatus::Unloaded:
            MIZU_UNREACHABLE("Trying to unload an already unloaded asset with handle: {}", handle.get_id());
            return false;
        case ResidencyStatus::Loading:
            expected_status = ResidencyStatus::Evicting;
            break;
        case ResidencyStatus::Loaded:
            expected_status = ResidencyStatus::Evicting;
            break;
        case ResidencyStatus::Evicting:
            MIZU_UNREACHABLE("Trying to unload an already evicting asset with handle: {}", handle.get_id());
            return false;
        }

        if (entry.status.compare_exchange_weak(
                current_status, expected_status, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return true;
        }
    }

    MIZU_UNREACHABLE("Failed to satisfy unload request");
    return false;
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
ResidencyStatus ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::get_status(
    AssetHandleType handle) const
{
    const ResidencyEntry& entry = get_residency_entry(handle);
    return entry.status.load(std::memory_order_acquire);
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
typename ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::ResidencySnapshot ResidencyTable<
    AssetHandleType,
    GpuPoolType,
    AllocationHandleType>::get_residency_snapshot(AssetHandleType handle) const
{
    const uint64_t handle_id = handle.get_id();
    const size_t shard_idx = handle_id % NumShards;

    const Shard& shard = m_slot_shards[shard_idx];

    std::shared_lock shard_lock{shard.mutex};

    const auto it = shard.handle_to_residency_entry.find(handle);
    MIZU_ASSERT(it != shard.handle_to_residency_entry.end(), "Residency entry not found for handle: {}", handle_id);

    ResidencySnapshot snapshot{};
    snapshot.status = it->second.status.load(std::memory_order_acquire);
    snapshot.allocation = it->second.allocation;

    return snapshot;
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
void ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::track_evictions()
{
    for (Shard& shard : m_slot_shards)
    {
        std::lock_guard shard_lock{shard.mutex};

        auto it = shard.handle_to_residency_entry.begin();
        while (it != shard.handle_to_residency_entry.end())
        {
            auto& [_, entry] = *it;

            if (entry.status.load(std::memory_order_acquire) == ResidencyStatus::Evicting)
            {
                entry.num_frames_in_eviction += 1;

                if (entry.num_frames_in_eviction >= EvictionThresholdFrames && entry.allocation.has_value())
                {
                    m_gpu_pool.free(*entry.allocation);
                    it = shard.handle_to_residency_entry.erase(it);

                    continue;
                }
            }

            it = std::next(it);
        }
    }
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
bool ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::transition_status(
    AssetHandleType handle,
    ResidencyStatus prev_status,
    ResidencyStatus new_status)
{
    ResidencyEntry& entry = get_residency_entry(handle);
    return entry.status.compare_exchange_strong(
        prev_status, new_status, std::memory_order_acq_rel, std::memory_order_acquire);
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
void ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::set_allocation(
    AssetHandleType handle,
    const AllocationHandleType& allocation)
{
    const uint64_t handle_id = handle.get_id();
    const size_t shard_idx = handle_id % NumShards;

    Shard& shard = m_slot_shards[shard_idx];

    std::lock_guard shard_lock{shard.mutex};

    const auto it = shard.handle_to_residency_entry.find(handle);
    MIZU_ASSERT(it != shard.handle_to_residency_entry.end(), "Residency entry not found for handle: {}", handle_id);

    it->second.allocation = allocation;
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
void ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::clear_allocation(AssetHandleType handle)
{
    const uint64_t handle_id = handle.get_id();
    const size_t shard_idx = handle_id % NumShards;

    Shard& shard = m_slot_shards[shard_idx];

    std::lock_guard shard_lock{shard.mutex};

    const auto it = shard.handle_to_residency_entry.find(handle);
    MIZU_ASSERT(it != shard.handle_to_residency_entry.end(), "Residency entry not found for handle: {}", handle_id);

    it->second.allocation.reset();
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
ResidencyEntryCpp& ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::get_or_create_residency_entry(
    AssetHandleType handle)
{
    const uint64_t handle_id = handle.get_id();
    const size_t shard_idx = handle_id % NumShards;

    Shard& shard = m_slot_shards[shard_idx];

    std::lock_guard shard_lock{shard.mutex};

    auto it = shard.handle_to_residency_entry.find(handle);
    if (it != shard.handle_to_residency_entry.end())
        return it->second;

    ResidencyEntry& entry = shard.handle_to_residency_entry.try_emplace(handle).first->second;
    entry.references.store(0, std::memory_order_relaxed);
    entry.status.store(ResidencyStatus::Unloaded, std::memory_order_release);
    entry.allocation.reset();
    entry.num_frames_in_eviction = 0;

    return entry;
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
ResidencyEntryCpp& ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::get_residency_entry(
    AssetHandleType handle)
{
    const uint64_t handle_id = handle.get_id();
    const size_t shard_idx = handle_id % NumShards;

    Shard& shard = m_slot_shards[shard_idx];

    std::shared_lock shard_lock{shard.mutex};

    const auto it = shard.handle_to_residency_entry.find(handle);

    MIZU_ASSERT(it != shard.handle_to_residency_entry.end(), "Residency entry not found for handle: {}", handle_id);

    return it->second;
}

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
const ResidencyEntryCpp& ResidencyTable<AssetHandleType, GpuPoolType, AllocationHandleType>::get_residency_entry(
    AssetHandleType handle) const
{
    const uint64_t handle_id = handle.get_id();
    const size_t shard_idx = handle_id % NumShards;

    const Shard& shard = m_slot_shards[shard_idx];

    std::shared_lock shard_lock{shard.mutex};

    const auto it = shard.handle_to_residency_entry.find(handle);
    MIZU_ASSERT(it != shard.handle_to_residency_entry.end(), "Residency entry not found for handle: {}", handle_id);

    return it->second;
}

#undef ResidencyEntryCpp

//
// ResidencyManager
//

static constexpr size_t MaxLoadJobs = 8;
static constexpr size_t MaxAssetsPerLoadJob = 16;
static constexpr size_t MinAssetsPerLoadJob = 2;

ResidencyManager::ResidencyManager(
    uint32_t frames_in_flight,
    IAssetLoader& asset_loader,
    CpuLoadingPool& cpu_loading_pool,
    GpuMeshPool& mesh_pool,
    GpuTexturePool& texture_pool)
    : m_frames_in_flight(frames_in_flight)
    , m_asset_loader(asset_loader)
    , m_cpu_loading_pool(cpu_loading_pool)
    , m_mesh_residency_table(mesh_pool)
    , m_texture_residency_table(texture_pool)
{
    MIZU_ASSERT(EvictionThresholdFrames >= m_frames_in_flight, "Eviction threadhold must be able to ");

    m_load_jobs_in_progress.resize(MaxLoadJobs * MaxAssetsPerLoadJob, LoadJobRecordPoolIndex{});

    for (size_t i = 0; i < MaxLoadJobs; ++i)
        available_jobs_in_progress_slots.push(i);

    static constexpr uint64_t StagingBufferSizePerFrame = 256 * 1024 * 1024; // 256 MB
    m_staging_buffer_manager = std::make_unique<StagingBufferManager>(frames_in_flight, StagingBufferSizePerFrame);
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
        m_load_jobs_queue_size.fetch_add(1, std::memory_order_relaxed);

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
        m_load_jobs_queue_size.fetch_add(1, std::memory_order_relaxed);

        return ResidencyStatus::Loading;
    }

    return ResidencyStatus::Unloaded;
}

bool ResidencyManager::request_unload(MeshAssetHandle handle)
{
    return m_mesh_residency_table.request_unload(handle);
}

bool ResidencyManager::request_unload(TextureAssetHandle handle)
{
    return m_texture_residency_table.request_unload(handle);
}

ResidencyStatus ResidencyManager::get_status(MeshAssetHandle handle) const
{
    return m_mesh_residency_table.get_status(handle);
}

ResidencyStatus ResidencyManager::get_status(TextureAssetHandle handle) const
{
    return m_texture_residency_table.get_status(handle);
}

ResidencyManager::MeshResidencySnapshot ResidencyManager::query_mesh_residency(MeshAssetHandle handle) const
{
    return m_mesh_residency_table.get_residency_snapshot(handle);
}

void ResidencyManager::prepare_frame(uint32_t frame_num)
{
    m_staging_buffer_manager->prepare_frame(frame_num);

    m_mesh_residency_table.track_evictions();
    m_texture_residency_table.track_evictions();
}

void ResidencyManager::dispatch_jobs()
{
    MIZU_PROFILE_SCOPED;

    const auto ceil_div = [](size_t numerator, size_t denominator) -> size_t {
        return (numerator + denominator - 1) / denominator;
    };

    if (m_load_jobs_queue_size.load(std::memory_order_relaxed) == 0
        || m_info.load_jobs_in_progress.load(std::memory_order_acquire) >= MaxLoadJobs)
        return;

    const size_t max_num_jobs_to_dispatch = MaxLoadJobs - m_info.load_jobs_in_progress.load(std::memory_order_acquire);
    const size_t num_requested_loads = m_load_jobs_queue_size.load(std::memory_order_relaxed);

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

            const size_t slot = available_jobs_in_progress_slots.front();
            available_jobs_in_progress_slots.pop();

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

            m_load_jobs_queue_size.fetch_sub(1, std::memory_order_relaxed);

            m_load_jobs_in_progress[jobs_in_progress_start + asset_index] = load_job_record_index;
        }

        m_info.load_jobs_in_progress.fetch_add(1, std::memory_order_release);
        g_job_system->schedule(&ResidencyManager::asset_load_job, this, jobs_in_progress_start, num_assets_for_this_job)
            .name("ResidencyManagerLoadJob")
            .submit();
    }
}

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

void ResidencyManager::add_streaming_resources_pass(RenderGraphBuilder& builder)
{
    static constexpr size_t MaxStreamingResourcesPerFrame = 16;

    if (m_resource_streaming_queue_size.load(std::memory_order_relaxed) == 0)
        return;

    const RenderGraphResource staging_buffer = builder.register_external_buffer(
        m_staging_buffer_manager->get_staging_buffer(),
        RenderGraphExternalBufferState{
            .initial_state = BufferResourceState::TransferSrc,
            .final_state = BufferResourceState::TransferSrc,
        });

    const RenderGraphResource mesh_vertex_buffer = builder.register_external_buffer(
        m_mesh_residency_table.m_gpu_pool.get_vertex_buffer(),
        RenderGraphExternalBufferState{
            .initial_state = BufferResourceState::TransferDst,
            .final_state = BufferResourceState::TransferDst,
        });

    const RenderGraphResource mesh_index_buffer = builder.register_external_buffer(
        m_mesh_residency_table.m_gpu_pool.get_index_buffer(),
        RenderGraphExternalBufferState{
            .initial_state = BufferResourceState::TransferDst,
            .final_state = BufferResourceState::TransferDst,
        });

    struct StreamResourcesPassData
    {
        RenderGraphResource staging_buffer;

        RenderGraphResource mesh_vertex_buffer;
        RenderGraphResource mesh_index_buffer;
    };

    builder.add_pass<StreamResourcesPassData>(
        "StreamResources",
        [&](RenderGraphPassBuilder& pass, StreamResourcesPassData& data) {
            pass.set_hint(RenderGraphPassHint::Transfer);

            data.staging_buffer = pass.write(staging_buffer);

            data.mesh_vertex_buffer = pass.write(mesh_vertex_buffer);
            data.mesh_index_buffer = pass.write(mesh_index_buffer);
        },
        [=,
         this](CommandBuffer& command, const StreamResourcesPassData& data, const RenderGraphPassResources& resources) {
            std::shared_ptr<BufferResource> vertex_buffer = resources.get_buffer(data.mesh_vertex_buffer);
            std::shared_ptr<BufferResource> index_buffer = resources.get_buffer(data.mesh_index_buffer);

            size_t num = 0;
            StreamingInfo streaming_info{};

            while (num < MaxStreamingResourcesPerFrame
                   && m_resource_streaming_queue_size.load(std::memory_order_relaxed) > 0
                   && m_resource_streaming_queue.pop(streaming_info))
            {
                std::visit(
                    overloaded{
                        [&](MeshAssetHandle&) {
                            stream_mesh_to_gpu(
                                command, streaming_info, *m_staging_buffer_manager, *vertex_buffer, *index_buffer);
                        },
                        [&](TextureAssetHandle&) {},
                    },
                    streaming_info.handle);

                num += 1;
                m_resource_streaming_queue_size.fetch_sub(1, std::memory_order_relaxed);
            }
        });
}

void ResidencyManager::asset_load_job(size_t jobs_in_progress_start, size_t num_assets)
{
    MIZU_PROFILE_SCOPED;

    for (size_t i = jobs_in_progress_start; i < jobs_in_progress_start + num_assets; ++i)
    {
        const LoadJobRecordPoolIndex load_job_record_index = m_load_jobs_in_progress[i];
        LoadJobRecord& record = m_load_job_record_pool.get(load_job_record_index);

        [[maybe_unused]] const bool loaded =
            std::visit([this](auto&& handle) { return load_asset(handle); }, record.handle);
        MIZU_ASSERT(loaded, "Failed to load asset in load job");

        process_load_job_completion(record);
    }

    {
        std::lock_guard lock{m_load_jobs_in_progress_mutex};
        available_jobs_in_progress_slots.push(jobs_in_progress_start / MaxAssetsPerLoadJob);
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

    const std::optional<MeshAssetRecord> asset_record = m_asset_loader.get_mesh_record(handle);
    if (!asset_record.has_value())
    {
        MIZU_LOG_ERROR("Failed to resolve mesh asset record for handle '{}'", handle.get_id());
        return false;
    }

    const std::optional<CpuAllocationHandle> allocation = allocate_asset(handle, asset_record->payload);
    if (!allocation.has_value())
    {
        MIZU_LOG_ERROR("Failed to load mesh asset for handle '{}'", handle.get_id());
        return false;
    }

    const std::optional<GpuMeshAllocationHandle> gpu_allocation = m_mesh_residency_table.m_gpu_pool.allocate(
        handle,
        asset_record->payload.get_vertex_data_size_bytes(),
        asset_record->payload.get_index_data_size_bytes(),
        alignof(MeshAssetVertex),
        asset_record->payload.get_index_data_alignment_bytes());

    if (!gpu_allocation.has_value())
    {
        m_cpu_loading_pool.abort_mesh(handle);
        MIZU_LOG_ERROR("Failed to reserve mesh asset for handle '{}' in GpuMeshPool", handle.get_id());
        return false;
    }

    m_mesh_residency_table.set_allocation(handle, *gpu_allocation);

    const StreamingInfo streaming_info{
        .handle = handle,
        .payload = asset_record->payload,
        .cpu_allocation = *allocation,
        .gpu_allocation = *gpu_allocation,
    };

    m_resource_streaming_queue.push(streaming_info);
    m_resource_streaming_queue_size.fetch_add(1, std::memory_order_relaxed);

    return true;
}

bool ResidencyManager::load_asset(TextureAssetHandle handle)
{
    MIZU_ASSERT(handle.is_valid(), "Trying to load invalid TextureAssetHandle");

    const std::optional<TextureAssetRecord> asset_record = m_asset_loader.get_texture_record(handle);
    if (!asset_record.has_value())
    {
        MIZU_LOG_ERROR("Failed to resolve texture asset record for handle '{}'", handle.get_id());
        return false;
    }

    const std::optional<CpuAllocationHandle> allocation = allocate_asset(handle, asset_record->payload);
    if (!allocation.has_value())
    {
        MIZU_LOG_ERROR("Failed to load texture asset for handle '{}'", handle.get_id());
        return false;
    }

    /* TODO:
    const StreamingInfo streaming_info{
        .handle = handle,
        .cpu_allocation = *allocation,
        .gpu_allocation = GpuTextureAllocationHandle{},
        .payload = asset_record->payload,
    };

    m_resource_streaming_queue.push(streaming_info);
    */

    return true;
}

std::optional<CpuAllocationHandle> ResidencyManager::allocate_asset(
    MeshAssetHandle handle,
    const MeshPayloadLayout& payload)
{
    if (const std::optional<CpuAllocationHandle> cached_allocation = m_cpu_loading_pool.get_mesh(handle))
        return cached_allocation;

    const CpuLoadAcquireResult acquire_result =
        m_cpu_loading_pool.acquire_mesh(handle, payload.size_bytes, payload.alignment);

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

std::optional<CpuAllocationHandle> ResidencyManager::allocate_asset(
    TextureAssetHandle handle,
    const TexturePayloadLayout& payload)
{
    MIZU_UNREACHABLE("Not implemented, well, partially :)");

    if (const std::optional<CpuAllocationHandle> cached_allocation = m_cpu_loading_pool.get_texture(handle))
        return cached_allocation;

    const CpuLoadAcquireResult acquire_result =
        m_cpu_loading_pool.acquire_texture(handle, payload.size_bytes, payload.alignment);

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

void ResidencyManager::stream_mesh_to_gpu(
    CommandBuffer& command,
    const StreamingInfo& info,
    StagingBufferManager& staging_manager,
    BufferResource& vertex_buffer,
    BufferResource& index_buffer)
{
    const MeshPayloadLayout* mesh_payload = std::get_if<MeshPayloadLayout>(&info.payload);
    if (mesh_payload == nullptr)
    {
        MIZU_LOG_ERROR("Invalid payload type in stream_mesh_to_gpu");
        return;
    }

    const GpuMeshAllocationHandle* gpu_allocation = std::get_if<GpuMeshAllocationHandle>(&info.gpu_allocation);
    if (gpu_allocation == nullptr)
    {
        MIZU_LOG_ERROR("Invalid GPU allocation type in stream_mesh_to_gpu");
        return;
    }

    if (!m_mesh_residency_table.transition_status(
            std::get<MeshAssetHandle>(info.handle), ResidencyStatus::Loading, ResidencyStatus::Loaded))
    {
        MIZU_LOG_ERROR(
            "Failed to transition residency status to Loaded for mesh asset '{}'",
            std::get<MeshAssetHandle>(info.handle).get_id());
        return;
    }

    const std::shared_ptr<BufferResource> staging_buffer = staging_manager.get_staging_buffer();

    const uint64_t offset = staging_manager.allocate(mesh_payload->size_bytes, 1);

    const uint64_t vertex_offset = offset;
    const uint64_t index_offset = offset + mesh_payload->get_vertex_data_size_bytes();

    const uint8_t* vertex_data = info.cpu_allocation.data.data();
    const uint8_t* index_data = info.cpu_allocation.data.data() + mesh_payload->get_vertex_data_size_bytes();

    staging_buffer->set_data(vertex_data, mesh_payload->get_vertex_data_size_bytes(), vertex_offset);
    staging_buffer->set_data(index_data, mesh_payload->get_index_data_size_bytes(), index_offset);

    const CopyBufferToBufferInfo vertex_copy_info{
        .size = mesh_payload->get_vertex_data_size_bytes(),
        .src_offset = vertex_offset,
        .dst_offset = gpu_allocation->vertex_offset,
    };

    const CopyBufferToBufferInfo index_copy_info{
        .size = mesh_payload->get_index_data_size_bytes(),
        .src_offset = index_offset,
        .dst_offset = gpu_allocation->index_offset,
    };

    command.copy_buffer_to_buffer(*staging_buffer, vertex_buffer, vertex_copy_info);
    command.copy_buffer_to_buffer(*staging_buffer, index_buffer, index_copy_info);
}

} // namespace Mizu
