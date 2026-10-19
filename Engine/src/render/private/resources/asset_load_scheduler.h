#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <span>
#include <variant>

#include "asset/asset_handle.h"
#include "core/job_system/mpsc_queue.h"

namespace Mizu
{

class IAssetLoader;

using MeshLoadingFinishedFunc = std::function<void(MeshAssetHandle)>;
using TextureLoadingFinishedFunc = std::function<void(TextureAssetHandle)>;

class AssetLoadScheduler
{
  public:
    AssetLoadScheduler(IAssetLoader& asset_loader);

    void dispatch_load_jobs();

    void request_mesh_load(
        MeshAssetHandle handle,
        uint32_t first_lod,
        uint32_t lod_count,
        std::span<uint8_t> destination,
        const MeshLoadingFinishedFunc& callback);
    void request_texture_load(
        TextureAssetHandle handle,
        uint32_t first_mip,
        uint32_t mip_count,
        std::span<uint8_t> destination,
        const TextureLoadingFinishedFunc& callback);

    uint32_t get_load_jobs_in_flight() const;

  private:
    IAssetLoader& m_asset_loader;

    std::atomic<uint32_t> m_load_jobs_in_flight{};

    using LoadingFinishedFunc = std::function<void(AssetKey)>;
    using AssetHandleT = std::variant<MeshAssetHandle, TextureAssetHandle>;

    struct LoadJobRecord
    {
        AssetHandleT asset_handle;
        uint32_t level_base;
        uint32_t level_count;
        std::span<uint8_t> destination;
        LoadingFinishedFunc callback_func;
    };

    static constexpr size_t LOAD_JOB_QUEUE_SIZE = 512;
    MpscQueue<LoadJobRecord, LOAD_JOB_QUEUE_SIZE> m_load_job_queue{};

    std::vector<LoadJobRecord> m_load_job_record_pool{};
    std::queue<uint32_t> m_load_job_record_pool_available_indices{};
    std::mutex m_load_job_record_pool_mutex{};

    void asset_load_job(uint32_t job_record_start_index, uint32_t num_assets);

    bool load_asset(const MeshAssetHandle& handle, const LoadJobRecord& job_record);
    bool load_asset(const TextureAssetHandle& handle, const LoadJobRecord& job_record);
};

} // namespace Mizu
