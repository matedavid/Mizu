#include "resources/asset_load_system.h"

#include <array>

#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/runtime.h"

namespace Mizu
{

static constexpr size_t MAX_LOAD_JOBS = 8;
static constexpr size_t MAX_ASSETS_PER_LOAD_JOB = 16;
static constexpr size_t MIN_ASSETS_PER_LOAD_JOB = 2;

AssetLoadSystem::AssetLoadSystem(IAssetLoader& asset_loader, CpuLoadingPool& cpu_loading_pool)
    : m_asset_loader(asset_loader)
    , m_cpu_loading_pool(cpu_loading_pool)
{
    m_load_job_record_pool.resize(MAX_LOAD_JOBS * MAX_ASSETS_PER_LOAD_JOB);

    for (size_t i = 0; i < MAX_LOAD_JOBS; ++i)
    {
        m_load_job_record_pool_available_indices.push(i);
    }
}

std::optional<MaterialAssetRecord> AssetLoadSystem::get_material_record(const MaterialAssetHandle& handle)
{
    return m_asset_loader.get_material_record(handle);
}

void AssetLoadSystem::dispatch_load_jobs()
{
    MIZU_PROFILE_SCOPED;

    const auto ceil_div = [](size_t numerator, size_t denominator) -> size_t {
        return (numerator + denominator - 1) / denominator;
    };

    const size_t num_load_jobs = m_load_job_queue_size.load(std::memory_order_relaxed);
    const size_t load_jobs_in_progress = m_load_jobs_in_progress.load(std::memory_order_relaxed);

    if (num_load_jobs == 0 || load_jobs_in_progress >= MAX_LOAD_JOBS)
        return;

    const size_t max_num_jobs_to_dispatch = MAX_LOAD_JOBS - load_jobs_in_progress;
    const size_t num_requested_loads = num_load_jobs;

    const size_t num_assets_to_dispatch =
        std::min(num_requested_loads, max_num_jobs_to_dispatch * MAX_ASSETS_PER_LOAD_JOB);

    const size_t min_num_jobs_for_max_batch_size = ceil_div(num_assets_to_dispatch, MAX_ASSETS_PER_LOAD_JOB);
    const size_t max_num_jobs_for_min_batch_size =
        num_assets_to_dispatch < MIN_ASSETS_PER_LOAD_JOB ? 1 : num_assets_to_dispatch / MIN_ASSETS_PER_LOAD_JOB;

    const size_t num_jobs_to_dispatch =
        std::max(min_num_jobs_for_max_batch_size, std::min(max_num_jobs_to_dispatch, max_num_jobs_for_min_batch_size));

    const size_t assets_per_job = num_assets_to_dispatch / num_jobs_to_dispatch;
    const size_t num_jobs_with_extra_asset = num_assets_to_dispatch % num_jobs_to_dispatch;

    std::array<size_t, MAX_LOAD_JOBS> num_assets_per_job{};
    std::fill(num_assets_per_job.begin(), num_assets_per_job.end(), assets_per_job);

    for (size_t job_index = 0; job_index < num_jobs_with_extra_asset; ++job_index)
        num_assets_per_job[job_index] += 1;

    for (size_t job_index = 0; job_index < num_jobs_to_dispatch; ++job_index)
    {
        const size_t num_assets_for_this_job = num_assets_per_job[job_index];

        const size_t job_in_progress_slot = [&]() {
            std::lock_guard lock{m_load_job_record_pool_mutex};

            const size_t slot = m_load_job_record_pool_available_indices.front();
            m_load_job_record_pool_available_indices.pop();

            return slot;
        }();

        const size_t jobs_in_progress_start = job_in_progress_slot * MAX_ASSETS_PER_LOAD_JOB;

        for (size_t asset_index = 0; asset_index < num_assets_for_this_job; ++asset_index)
        {
            LoadJobRecord load_job_record_index{};
            if (!m_load_job_queue.pop(load_job_record_index))
            {
                MIZU_ASSERT(false, "Failed to pop from load jobs queue while dispatching jobs");
                return;
            }

            m_load_job_queue_size.fetch_sub(1, std::memory_order_relaxed);

            m_load_job_record_pool[jobs_in_progress_start + asset_index] = load_job_record_index;
        }

        m_load_jobs_in_progress.fetch_add(1, std::memory_order_release);
        g_job_system->schedule(&AssetLoadSystem::asset_load_job, this, jobs_in_progress_start, num_assets_for_this_job)
            .name("AssetLoadSystem_LoadJob")
            .submit();
    }
}

void AssetLoadSystem::request_mesh_load(
    const MeshAssetHandle& handle,
    MeshCpuLoadingFinishedFunc cpu_finished_callback,
    MeshGpuLoadingFinishedFunc gpu_finished_callback)
{
    m_load_job_queue.push({
        .handle = handle,
        .cpu_finished_callback = cpu_finished_callback,
        .gpu_finished_callback = gpu_finished_callback,
    });

    m_load_job_queue_size.fetch_add(1, std::memory_order_acq_rel);
}

void AssetLoadSystem::request_texture_load(
    const TextureAssetHandle& handle,
    TextureCpuLoadingFinishedFunc cpu_finished_callback,
    TextureGpuLoadingFinishedFunc gpu_finished_callback)
{
    m_load_job_queue.push({
        .handle = handle,
        .cpu_finished_callback = cpu_finished_callback,
        .gpu_finished_callback = gpu_finished_callback,
    });

    m_load_job_queue_size.fetch_add(1, std::memory_order_acq_rel);
}

void AssetLoadSystem::asset_load_job(size_t job_record_start_index, size_t num_assets)
{
    MIZU_PROFILE_SCOPED;

    for (size_t i = job_record_start_index; i < job_record_start_index + num_assets; ++i)
    {
        const LoadJobRecord& job_record = m_load_job_record_pool[i];

        const bool loaded =
            std::visit([&](const auto& handle) { return load_asset(handle, job_record); }, job_record.handle);
        if (!loaded)
        {
            MIZU_ASSERT(false, "Failed to load asset");
            continue;
        }

        m_load_job_record_pool[i] = LoadJobRecord{};
    }

    {
        std::lock_guard lock{m_load_job_record_pool_mutex};
        m_load_job_record_pool_available_indices.push(job_record_start_index / MAX_ASSETS_PER_LOAD_JOB);
    }

    m_load_jobs_in_progress.fetch_sub(1, std::memory_order_release);
}

bool AssetLoadSystem::load_asset(const MeshAssetHandle& handle, const LoadJobRecord& job_record)
{
    const std::optional<MeshAssetRecord> record = m_asset_loader.get_mesh_record(handle);
    if (!record.has_value())
    {
        MIZU_LOG_ERROR("Failed to get mesh record for handle: {}", handle.get_id());
        return false;
    }

    const CpuLoadAcquireResult result = m_cpu_loading_pool.acquire_mesh(
        handle, record->payload.get_total_size_bytes(), record->payload.get_total_alignment_bytes());

    if (!result.allocation.is_valid())
    {
        MIZU_LOG_ERROR(
            "Failed to acquire cpu loading pool allocation for mesh handle: {}, size: {}, alignment: {}",
            handle.get_id(),
            record->payload.get_total_size_bytes(),
            record->payload.get_total_alignment_bytes());

        return false;
    }

    bool should_load = false;
    if (!load_cpu_data(result, should_load))
    {
        MIZU_LOG_ERROR("Failed to acquire cpu loading pool allocation for mesh handle: {}", handle.get_id());
        return false;
    }

    if (should_load)
    {
        if (!m_asset_loader.load_mesh_payload(handle, result.allocation.data))
        {
            m_cpu_loading_pool.abort_mesh(handle);

            MIZU_LOG_ERROR("Failed to load mesh payload for handle: {}", handle.get_id());
            return false;
        }

        m_cpu_loading_pool.commit_mesh(handle);
    }

    const MeshCpuLoadingFinishedFunc& cpu_callback =
        std::get<MeshCpuLoadingFinishedFunc>(job_record.cpu_finished_callback);
    cpu_callback(handle, result.allocation);

    m_gpu_upload_queue.push({
        .cpu_result = result,
        .payload = *record,
        .gpu_finished_callback = job_record.gpu_finished_callback,
    });

    return true;
}

bool AssetLoadSystem::load_asset(const TextureAssetHandle& handle, const LoadJobRecord& job_record)
{
    const std::optional<TextureAssetRecord> record = m_asset_loader.get_texture_record(handle);
    if (!record.has_value())
    {
        MIZU_LOG_ERROR("Failed to get texture record for handle: {}", handle.get_id());
        return false;
    }

    const CpuLoadAcquireResult result =
        m_cpu_loading_pool.acquire_texture(handle, record->payload.get_total_size_bytes());

    if (!result.allocation.is_valid())
    {
        MIZU_LOG_ERROR(
            "Failed to acquire cpu loading pool allocation for texture handle: {}, size: {}",
            handle.get_id(),
            record->payload.get_total_size_bytes());
        return false;
    }

    bool should_load = false;
    if (!load_cpu_data(result, should_load))
    {
        MIZU_LOG_ERROR("Failed to acquire cpu loading pool allocation for texture handle: {}", handle.get_id());
        return false;
    }

    if (should_load)
    {
        if (!m_asset_loader.load_texture_payload(handle, result.allocation.data))
        {
            m_cpu_loading_pool.abort_texture(handle);

            MIZU_LOG_ERROR("Failed to load texture payload for handle: {}", handle.get_id());
            return false;
        }

        m_cpu_loading_pool.commit_texture(handle);
    }

    const TextureCpuLoadingFinishedFunc& cpu_callback =
        std::get<TextureCpuLoadingFinishedFunc>(job_record.cpu_finished_callback);
    cpu_callback(handle, result.allocation);

    m_gpu_upload_queue.push({
        .cpu_result = result,
        .payload = *record,
        .gpu_finished_callback = job_record.gpu_finished_callback,
    });

    return true;
}

bool AssetLoadSystem::load_cpu_data(const CpuLoadAcquireResult& result, bool& should_load)
{
    if (result.status == CpuLoadAcquireStatus::Failed || result.status == CpuLoadAcquireStatus::PendingLoad)
    {
        should_load = false;
        return false;
    }

    should_load = result.status == CpuLoadAcquireStatus::LoadRequired;
    return true;
}

} // namespace Mizu