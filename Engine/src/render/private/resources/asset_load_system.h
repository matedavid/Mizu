#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "core/job_system/mpsc_queue.h"
#include "render_core/rhi/buffer_resource.h"

#include "render/render_graph/render_graph_builder.h"
#include "render/resources/gpu_pools_handles.h"
#include "render/runtime/renderer.h"
#include "resources/cpu_loading_pool.h"

namespace Mizu
{

class CommandBuffer;
class GpuMeshPool;
class GpuTexturePool;

using GpuAllocationHandle = std::variant<GpuMeshAllocationHandle, GpuTextureAllocationHandle>;

using MeshCpuLoadingFinishedFunc =
    std::function<void(const MeshAssetHandle& handle, const CpuAllocationHandle& allocation_handle)>;
using MeshGpuLoadingFinishedFunc =
    std::function<void(const MeshAssetHandle& handle, const GpuMeshAllocationHandle& allocation_handle)>;

using TextureCpuLoadingFinishedFunc =
    std::function<void(const TextureAssetHandle& handle, const CpuAllocationHandle& allocation_handle)>;
using TextureGpuLoadingFinishedFunc =
    std::function<void(const TextureAssetHandle& handle, const GpuTextureAllocationHandle& allocation_handle)>;

class AssetLoadSystem
{
  public:
    AssetLoadSystem(
        IAssetLoader& asset_loader,
        CpuLoadingPool& cpu_loading_pool,
        GpuMeshPool& gpu_mesh_pool,
        GpuTexturePool& gpu_texture_pool);

    void dispatch_load_jobs();
    void build_gpu_uploads(RenderGraphBuilder& builder);

    std::optional<MaterialAssetRecord> get_material_record(const MaterialAssetHandle& handle);

    void request_mesh_load(
        const MeshAssetHandle& handle,
        MeshCpuLoadingFinishedFunc cpu_finished_callback,
        MeshGpuLoadingFinishedFunc gpu_finished_callback);
    void request_texture_load(
        const TextureAssetHandle& handle,
        TextureCpuLoadingFinishedFunc cpu_finished_callback,
        TextureGpuLoadingFinishedFunc gpu_finished_callback);

  private:
    IAssetLoader& m_asset_loader;
    CpuLoadingPool& m_cpu_loading_pool;
    GpuMeshPool& m_gpu_mesh_pool;
    GpuTexturePool& m_gpu_texture_pool;

    using AssetHandleT = std::variant<MeshAssetHandle, TextureAssetHandle>;
    using CpuLoadingFinishedFunc = std::variant<MeshCpuLoadingFinishedFunc, TextureCpuLoadingFinishedFunc>;
    using GpuLoadingFinishedFunc = std::variant<MeshGpuLoadingFinishedFunc, TextureGpuLoadingFinishedFunc>;

    struct LoadJobRecord
    {
        AssetHandleT handle;
        CpuLoadingFinishedFunc cpu_finished_callback;
        GpuLoadingFinishedFunc gpu_finished_callback;
    };

    static constexpr size_t LOAD_JOB_QUEUE_SIZE = 128;

    MpscQueue<LoadJobRecord, LOAD_JOB_QUEUE_SIZE> m_load_job_queue{};
    std::atomic<size_t> m_load_job_queue_size{0};

    std::vector<LoadJobRecord> m_load_job_record_pool{};
    std::queue<size_t> m_load_job_record_pool_available_indices{};
    std::mutex m_load_job_record_pool_mutex{};

    std::atomic<size_t> m_load_jobs_in_progress{0};

    using AssetRecordT = std::variant<MeshAssetRecord, TextureAssetRecord>;
    using GpuAllocationHandle = std::variant<GpuMeshAllocationHandle, GpuTextureAllocationHandle>;
    struct GpuUploadRecord
    {
        CpuLoadAcquireResult cpu_result;
        GpuAllocationHandle gpu_allocation;
        AssetRecordT record;
        GpuLoadingFinishedFunc gpu_finished_callback;
    };

    MpscQueue<GpuUploadRecord, LOAD_JOB_QUEUE_SIZE> m_gpu_upload_queue{};
    std::atomic<size_t> m_gpu_upload_queue_size{0};

    class UploadStagingBuffer
    {
      public:
        struct Allocation
        {
            BufferResource* buffer = nullptr;
            uint8_t* mapped_data = nullptr;
            uint64_t offset = 0;
            uint64_t size = 0;
        };

        void init(uint32_t frames_in_flight, uint64_t bytes_per_frame, std::string_view name)
        {
            MIZU_ASSERT(frames_in_flight > 0, "UploadStagingBuffer requires at least one frame in flight");
            MIZU_ASSERT(bytes_per_frame > 0, "UploadStagingBuffer requires a non-zero size per frame");

            m_frames_in_flight = frames_in_flight;
            m_bytes_per_frame = bytes_per_frame;

            const uint64_t total_bytes = m_bytes_per_frame * m_frames_in_flight;
            BufferDescription staging_desc = create_staging_buffer_desc(total_bytes, std::string{name});

            m_buffer = g_render_device->create_buffer(staging_desc);
            MIZU_ASSERT(m_buffer != nullptr, "Failed to create upload staging buffer");

            m_mapped_data = m_buffer->map();
            MIZU_ASSERT(m_mapped_data != nullptr, "Failed to map upload staging buffer");

            m_frame_idx = m_frames_in_flight - 1;
            m_frame_base_offset = 0;
            m_head = 0;
        }

        void begin_frame()
        {
            m_frame_idx = (m_frame_idx + 1) % m_frames_in_flight;
            m_frame_base_offset = static_cast<uint64_t>(m_frame_idx) * m_bytes_per_frame;
            m_head = m_frame_base_offset;
        }

        Allocation allocate(uint64_t size, uint64_t alignment)
        {
            const uint64_t aligned_head = align_up_u64(m_head, alignment);

            MIZU_ASSERT(
                aligned_head + size <= m_frame_base_offset + m_bytes_per_frame,
                "UploadStagingBuffer overflow: requested {} bytes with alignment {} from head {} (capacity {})",
                size,
                alignment,
                m_head,
                m_bytes_per_frame);

            Allocation allocation{};
            allocation.buffer = m_buffer.get();
            allocation.mapped_data = m_mapped_data;
            allocation.offset = aligned_head;
            allocation.size = size;

            m_head = aligned_head + size;
            return allocation;
        }

        const std::shared_ptr<BufferResource>& get_buffer() const { return m_buffer; }

      private:
        std::shared_ptr<BufferResource> m_buffer{};
        uint8_t* m_mapped_data = nullptr;

        uint32_t m_frames_in_flight = 1;
        uint64_t m_bytes_per_frame = 0;

        uint32_t m_frame_idx = 0;
        uint64_t m_frame_base_offset = 0;
        uint64_t m_head = 0;

        static uint64_t align_up_u64(uint64_t value, uint64_t alignment)
        {
            MIZU_ASSERT(alignment > 0, "Alignment must be greater than zero");
            return ((value + alignment - 1) / alignment) * alignment;
        }
    };

    UploadStagingBuffer m_upload_staging{};

    void asset_load_job(size_t job_record_start_index, size_t num_assets);

    bool load_asset(const MeshAssetHandle& handle, const LoadJobRecord& job_record);
    bool load_asset(const TextureAssetHandle& handle, const LoadJobRecord& job_record);

    bool load_cpu_data(const CpuLoadAcquireResult& result, bool& should_load);

    void upload_gpu(CommandBuffer& command, const MeshAssetRecord& record, const GpuUploadRecord& upload);
    void upload_gpu(CommandBuffer& command, const TextureAssetRecord& record, const GpuUploadRecord& upload);
};

} // namespace Mizu