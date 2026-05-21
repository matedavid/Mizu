#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <unordered_map>
#include <variant>
#include <vector>

#include "asset/asset_handle.h"
#include "asset/asset_loader.h"
#include "base/debug/logging.h"
#include "core/job_system/intrusive_free_list.h"
#include "core/job_system/mpsc_queue.h"

#include "render/render_graph/render_graph_builder.h"
#include "resources/cpu_loading_pool.h"
#include "resources/gpu_pools.h"

namespace Mizu
{

enum class ResidencyStatus
{
    Unloaded,
    Loading,
    Loaded,
    Evicting,
};

template <typename AssetHandleType, typename GpuPoolType, typename AllocationHandleType>
class ResidencyTable
{
    static_assert(IsAssetHandleType<AssetHandleType>, "ResidencyTable can only be instantiated with AssetHandle types");

  public:
    struct ResidencySnapshot
    {
        ResidencyStatus status = ResidencyStatus::Unloaded;
        std::optional<AllocationHandleType> allocation{};
    };

    ResidencyTable(GpuPoolType& gpu_pool);

    ResidencyStatus request_load(AssetHandleType handle, bool& out_enqueue_job);
    bool request_unload(AssetHandleType handle);
    ResidencyStatus get_status(AssetHandleType handle) const;
    ResidencySnapshot get_residency_snapshot(AssetHandleType handle) const;
    void track_evictions();

  private:
    GpuPoolType& m_gpu_pool;

    struct ResidencyEntry
    {
        std::atomic<uint32_t> references;
        std::atomic<ResidencyStatus> status;
        std::optional<AllocationHandleType> allocation{};
        uint32_t num_frames_in_eviction;
    };

    struct Shard
    {
        mutable std::shared_mutex mutex;
        std::unordered_map<AssetHandleType, ResidencyEntry> handle_to_residency_entry{};
    };

    static constexpr size_t NumShards = 16;
    std::array<Shard, NumShards> m_slot_shards{};

    bool transition_status(AssetHandleType handle, ResidencyStatus prev_status, ResidencyStatus new_status);
    void set_allocation(AssetHandleType handle, const AllocationHandleType& allocation);
    void clear_allocation(AssetHandleType handle);
    ResidencyEntry& get_or_create_residency_entry(AssetHandleType handle);

    ResidencyEntry& get_residency_entry(AssetHandleType handle);
    const ResidencyEntry& get_residency_entry(AssetHandleType handle) const;

    friend class ResidencyManager;
};

class ResidencyManager
{
    using MeshResidencyTable = ResidencyTable<MeshAssetHandle, GpuMeshPool, GpuMeshAllocationHandle>;
    using TextureResidencyTable = ResidencyTable<TextureAssetHandle, GpuTexturePool, GpuTextureAllocationHandle>;

  public:
    using MeshResidencySnapshot = MeshResidencyTable::ResidencySnapshot;

    ResidencyManager(
        uint32_t frames_in_flight,
        IAssetLoader& asset_loader,
        CpuLoadingPool& cpu_loading_pool,
        GpuMeshPool& mesh_pool,
        GpuTexturePool& texture_pool);

    ResidencyStatus request_load(MeshAssetHandle handle);
    ResidencyStatus request_load(TextureAssetHandle handle);

    bool request_unload(MeshAssetHandle handle);
    bool request_unload(TextureAssetHandle handle);

    ResidencyStatus get_status(MeshAssetHandle handle) const;
    ResidencyStatus get_status(TextureAssetHandle handle) const;

    MeshResidencySnapshot query_mesh_residency(MeshAssetHandle handle) const;

    void prepare_frame(uint32_t frame_num);
    void dispatch_jobs();
    void add_streaming_resources_pass(RenderGraphBuilder& builder);

  private:
    uint32_t m_frames_in_flight = 0;

    IAssetLoader& m_asset_loader;
    CpuLoadingPool& m_cpu_loading_pool;

    MeshResidencyTable m_mesh_residency_table;
    TextureResidencyTable m_texture_residency_table;

    struct ResidencyManagerInfo
    {
        std::atomic<uint32_t> load_jobs_in_progress = 0;
    };
    ResidencyManagerInfo m_info{};

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

    static constexpr size_t PoolCapacity = 1024;
    IntrusiveFreeList<LoadJobRecord, PoolCapacity, LoadJobRecordPoolTag> m_load_job_record_pool;
    IntrusiveFreeList<WaitNode, PoolCapacity, WaitNodePoolTag> m_wait_node_pool;

    MpscQueue<LoadJobRecordPoolIndex, PoolCapacity> m_load_jobs_queue;
    std::atomic<size_t> m_load_jobs_queue_size{0};

    using GpuAllocationHandle = std::variant<GpuMeshAllocationHandle, GpuTextureAllocationHandle>;
    using AssetPayload = std::variant<MeshPayloadLayout, TexturePayloadLayout>;

    struct StreamingInfo
    {
        AssetHandle handle{};
        AssetPayload payload{};

        CpuAllocationHandle cpu_allocation{};
        GpuAllocationHandle gpu_allocation{};
    };

    MpscQueue<StreamingInfo, PoolCapacity> m_resource_streaming_queue;
    std::atomic<size_t> m_resource_streaming_queue_size{0};

    std::vector<LoadJobRecordPoolIndex> m_load_jobs_in_progress;
    std::queue<size_t> available_jobs_in_progress_slots;
    std::mutex m_load_jobs_in_progress_mutex;

    class StagingBufferManager
    {
      public:
        StagingBufferManager(uint32_t frames_in_flight, uint64_t buffer_size_per_frame)
            : m_frames_in_flight(frames_in_flight)
            , m_buffer_size_per_frame(buffer_size_per_frame)
        {
            const BufferDescription staging_buffer_desc =
                create_staging_buffer_desc(m_buffer_size_per_frame * m_frames_in_flight);
            m_staging_buffer = g_render_device->create_buffer(staging_buffer_desc);
        }

        void prepare_frame(uint32_t frame_num)
        {
            m_current_frame_offset = (frame_num % m_frames_in_flight) * m_buffer_size_per_frame;
        }

        uint64_t allocate(uint64_t size, uint64_t alignment)
        {
            const uint64_t aligned_offset = (m_current_frame_offset + alignment - 1) & ~(alignment - 1); // align up

            if (aligned_offset + size > m_current_frame_offset + m_buffer_size_per_frame)
            {
                MIZU_LOG_ERROR(
                    "StagingBufferManager: Out of memory for current frame. Requested size: {}, alignment: {}",
                    size,
                    alignment);

                return std::numeric_limits<uint64_t>::max();
            }

            m_current_frame_offset = aligned_offset + size;
            return aligned_offset;
        }

        std::shared_ptr<BufferResource> get_staging_buffer() const { return m_staging_buffer; }

      private:
        uint32_t m_frames_in_flight = 0;
        uint64_t m_buffer_size_per_frame = 0;

        uint64_t m_current_frame_offset = 0;

        std::shared_ptr<BufferResource> m_staging_buffer;
    };

    std::unique_ptr<StagingBufferManager> m_staging_buffer_manager;

    void asset_load_job(size_t jobs_in_progress_start, size_t num_assets);
    void process_load_job_completion(LoadJobRecord& record);

    bool load_asset(MeshAssetHandle handle);
    bool load_asset(TextureAssetHandle handle);

    std::optional<CpuAllocationHandle> allocate_asset(MeshAssetHandle handle, const MeshPayloadLayout& payload);
    std::optional<CpuAllocationHandle> allocate_asset(TextureAssetHandle handle, const TexturePayloadLayout& payload);

    void stream_mesh_to_gpu(
        CommandBuffer& command,
        const StreamingInfo& info,
        StagingBufferManager& staging_manager,
        BufferResource& vertex_buffer,
        BufferResource& index_buffer);
};

} // namespace Mizu