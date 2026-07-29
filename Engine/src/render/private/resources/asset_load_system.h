#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <variant>
#include <vector>

#include "asset/asset_loader.h"
#include "core/job_system/mpsc_queue.h"

#include "render/render_graph/render_graph_builder.h"
#include "render/resources/gpu_resource_types.h"
#include "resources/cpu_loading_pool.h"

namespace Mizu
{

class CommandBuffer;
class FrameLinearAllocator;
class GpuMeshPool;
class GpuTexturePool;

using GpuAllocationHandle = std::variant<GpuMeshAllocationHandle, GpuTextureAllocationHandle>;

using MeshCpuLoadingFinishedFunc =
    std::function<void(const MeshAssetHandle& handle, const CpuAllocationHandle& allocation_handle)>;
using MeshGpuLoadingFinishedFunc =
    std::function<void(const MeshAssetHandle& handle, const GpuMeshResidentRecord& resident_record)>;

using TextureCpuLoadingFinishedFunc =
    std::function<void(const TextureAssetHandle& handle, const CpuAllocationHandle& allocation_handle)>;
using TextureGpuLoadingFinishedFunc =
    std::function<void(const TextureAssetHandle& handle, const GpuTextureResidentRecord& resident_record)>;

class AssetLoadSystem
{
  public:
    AssetLoadSystem(
        IAssetLoader& asset_loader,
        CpuLoadingPool& cpu_loading_pool,
        GpuMeshPool& gpu_mesh_pool,
        GpuTexturePool& gpu_texture_pool);

    void dispatch_load_jobs();
    void add_gpu_uploads_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);

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

    void asset_load_job(size_t job_record_start_index, size_t num_assets);

    bool load_asset(const MeshAssetHandle& handle, const LoadJobRecord& job_record);
    bool load_asset(const TextureAssetHandle& handle, const LoadJobRecord& job_record);

    bool load_cpu_data(const CpuLoadAcquireResult& result, bool& should_load);

    void upload_gpu(
        CommandBuffer& command,
        FrameLinearAllocator& frame_allocator,
        const MeshAssetRecord& record,
        const GpuUploadRecord& upload);
    void upload_gpu(
        CommandBuffer& command,
        FrameLinearAllocator& frame_allocator,
        const TextureAssetRecord& record,
        const GpuUploadRecord& upload);
};

} // namespace Mizu