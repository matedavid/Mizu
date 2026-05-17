#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <unordered_map>
#include <variant>
#include <vector>

#include "asset/asset_handle.h"
#include "asset/asset_loader.h"
#include "core/job_system/intrusive_free_list.h"
#include "core/job_system/mpsc_queue.h"
#include "resources/cpu_loading_pool.h"

namespace Mizu
{

class GpuMeshPool;
class GpuTexturePool;

enum class ResidencyStatus
{
    Unloaded,
    Loading,
    Loaded,
    Evicting,
};

template <typename AssetHandleType, typename GpuPoolType>
class ResidencyTable
{
    static_assert(IsAssetHandleType<AssetHandleType>, "ResidencyTable can only be instantiated with AssetHandle types");

  public:
    ResidencyTable(GpuPoolType& gpu_pool);

    ResidencyStatus request_load(AssetHandleType handle, bool& out_enqueue_job);
    ResidencyStatus get_status(AssetHandleType handle) const;

  private:
    GpuPoolType& m_gpu_pool;

    struct ResidencyEntryIndex
    {
        uint32_t index;
        uint32_t generation;
    };

    static_assert(sizeof(ResidencyEntryIndex) == sizeof(uint64_t), "ResidencyEntryIndex must be 8 bytes");

    struct Shard
    {
        mutable std::shared_mutex mutex;
        std::unordered_map<AssetHandleType, ResidencyEntryIndex> handle_to_entry_slot_map{};
    };

    static constexpr size_t NumShards = 16;
    std::array<Shard, NumShards> m_slot_shards{};

    struct ResidencyEntry
    {
        uint32_t generation;
        std::atomic<uint32_t> references;

        std::atomic<ResidencyStatus> status;
        uint32_t last_used_frame;
    };

    std::vector<ResidencyEntry> m_entries;

    // Used to lock the entries array when reallocating
    mutable std::mutex m_entries_mutex;

    std::optional<ResidencyEntryIndex> get_entry_index(const AssetHandleType& handle) const;
};

class ResidencyManager
{
  public:
    ResidencyManager(
        IAssetLoader& asset_loader,
        CpuLoadingPool& cpu_loading_pool,
        GpuMeshPool& mesh_pool,
        GpuTexturePool& texture_pool);

    ResidencyStatus request_load(MeshAssetHandle handle);
    ResidencyStatus request_load(TextureAssetHandle handle);

    ResidencyStatus get_status(MeshAssetHandle handle) const;
    ResidencyStatus get_status(TextureAssetHandle handle) const;

    void dispatch_jobs();

  private:
    IAssetLoader& m_asset_loader;
    CpuLoadingPool& m_cpu_loading_pool;
    ResidencyTable<MeshAssetHandle, GpuMeshPool> m_mesh_residency_table;
    ResidencyTable<TextureAssetHandle, GpuTexturePool> m_texture_residency_table;

    struct ResidencyManagerInfo
    {
        std::atomic<uint32_t> load_jobs_in_progress = 0;
    };
    ResidencyManagerInfo m_info{};

    static constexpr size_t PoolCapacity = 1024;

    using AssetHandle = std::variant<MeshAssetHandle, TextureAssetHandle>;

    struct LoadJobRecordPoolTag;
    using LoadJobRecordPoolIndex = IntrusiveFreeListIndex<LoadJobRecordPoolTag>;

    struct WaitNodePoolTag;
    using WaitNodePoolIndex = IntrusiveFreeListIndex<WaitNodePoolTag>;

    struct LoadJobRecord
    {
        LoadJobRecordPoolIndex pool_index;
        LoadJobRecordPoolIndex next_free;

        AssetHandle handle{};
        std::atomic<uint32_t> num_dependencies{0};

        // -1 because max is used as the invalid index
        static constexpr WaitNodePoolIndex ClosedWaitingList =
            WaitNodePoolIndex{std::numeric_limits<WaitNodePoolIndex::ValueT>::max() - 1};
        std::atomic<WaitNodePoolIndex> waiting_jobs_head{};
    };

    struct WaitNode
    {
        WaitNodePoolIndex pool_index;
        WaitNodePoolIndex next_free;

        WaitNodePoolIndex next{};
        LoadJobRecordPoolIndex target_load_job_index{};
    };

    IntrusiveFreeList<LoadJobRecord, PoolCapacity, LoadJobRecordPoolTag> m_load_job_record_pool;
    IntrusiveFreeList<WaitNode, PoolCapacity, WaitNodePoolTag> m_wait_node_pool;

    MpscQueue<LoadJobRecordPoolIndex, PoolCapacity> m_load_jobs_queue;
    std::atomic<size_t> m_num_load_jobs_in_queue{0};

    struct StreamingInfo
    {
        AssetHandle handle;
        CpuAllocationHandle cpu_allocation{};
        // TODO: destination gpu allocation handle
    };

    MpscQueue<StreamingInfo, PoolCapacity> m_resource_streaming_queue;

    std::vector<LoadJobRecordPoolIndex> m_load_jobs_in_progress;
    std::queue<size_t> available_job_in_progress_slots;

    std::mutex m_load_jobs_in_progress_mutex;

    void asset_load_job(size_t jobs_in_progress_start, size_t num_assets);
    void process_load_job_completion(LoadJobRecord& record);

    bool load_asset(MeshAssetHandle handle);
    bool load_asset(TextureAssetHandle handle);

    std::optional<CpuAllocationHandle> allocate_asset(MeshAssetHandle handle);
    std::optional<CpuAllocationHandle> allocate_asset(TextureAssetHandle handle);
};

} // namespace Mizu