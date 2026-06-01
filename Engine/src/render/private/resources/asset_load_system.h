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

#include "render/resources/gpu_pools_handles.h"
#include "resources/cpu_loading_pool.h"

namespace Mizu
{

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
    AssetLoadSystem(IAssetLoader& asset_loader, CpuLoadingPool& cpu_loading_pool);

    void dispatch_load_jobs();

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

    using AssetPayloadT = std::variant<MeshAssetRecord, TextureAssetRecord>;
    struct GpuUploadRecord
    {
        CpuLoadAcquireResult cpu_result;
        AssetPayloadT payload;
        GpuLoadingFinishedFunc gpu_finished_callback;
    };

    MpscQueue<GpuUploadRecord, LOAD_JOB_QUEUE_SIZE> m_gpu_upload_queue{};
    std::atomic<size_t> m_gpu_upload_queue_size{0};

    void asset_load_job(size_t job_record_start_index, size_t num_assets);

    bool load_asset(const MeshAssetHandle& handle, const LoadJobRecord& job_record);
    bool load_asset(const TextureAssetHandle& handle, const LoadJobRecord& job_record);

    bool load_cpu_data(const CpuLoadAcquireResult& result, bool& should_load);
};

} // namespace Mizu