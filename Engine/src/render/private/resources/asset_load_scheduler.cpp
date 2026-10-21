#include "resources/asset_load_scheduler.h"

#include "asset/asset_loader.h"
#include "base/debug/profiling.h"
#include "base/math/math.h"
#include "core/runtime.h"

namespace Mizu
{

static constexpr uint32_t MAX_LOAD_JOBS = 8;
static constexpr uint32_t MAX_ASSETS_PER_LOAD_JOB = 8;
static constexpr uint32_t MIN_ASSETS_PER_LOAD_JOB = 1;

AssetLoadScheduler::AssetLoadScheduler(IAssetLoader& asset_loader) : m_asset_loader(asset_loader)
{
    m_load_job_record_pool.resize(MAX_LOAD_JOBS * MAX_ASSETS_PER_LOAD_JOB);

    for (uint32_t i = 0; i < MAX_LOAD_JOBS; ++i)
    {
        m_load_job_record_pool_available_indices.push(i);
    }
}

void AssetLoadScheduler::dispatch_load_jobs()
{
    MIZU_PROFILE_SCOPED;

    const uint32_t num_load_jobs = static_cast<uint32_t>(m_load_job_queue.size());

    if (num_load_jobs == 0)
        return;

    while (m_load_jobs_in_flight.load(std::memory_order_relaxed) >= MAX_LOAD_JOBS)
    {
        g_job_system->yield();
    }

    const uint32_t load_jobs_in_flight = m_load_jobs_in_flight.load(std::memory_order_acquire);

    const uint32_t max_num_jobs_to_dispatch = MAX_LOAD_JOBS - load_jobs_in_flight;
    const uint32_t num_requested_loads = num_load_jobs;

    const uint32_t num_assets_to_dispatch =
        std::min(num_requested_loads, max_num_jobs_to_dispatch * MAX_ASSETS_PER_LOAD_JOB);

    const uint32_t min_num_jobs_for_max_batch_size = math::ceil_div(num_assets_to_dispatch, MAX_ASSETS_PER_LOAD_JOB);
    const uint32_t max_num_jobs_for_min_batch_size =
        num_assets_to_dispatch < MIN_ASSETS_PER_LOAD_JOB ? 1 : num_assets_to_dispatch / MIN_ASSETS_PER_LOAD_JOB;

    const uint32_t num_jobs_to_dispatch =
        std::max(min_num_jobs_for_max_batch_size, std::min(max_num_jobs_to_dispatch, max_num_jobs_for_min_batch_size));

    const uint32_t assets_per_job = num_assets_to_dispatch / num_jobs_to_dispatch;
    const uint32_t num_jobs_with_extra_asset = num_assets_to_dispatch % num_jobs_to_dispatch;

    std::array<uint32_t, MAX_LOAD_JOBS> num_assets_per_job{};
    std::fill(num_assets_per_job.begin(), num_assets_per_job.end(), assets_per_job);

    for (uint32_t job_index = 0; job_index < num_jobs_with_extra_asset; ++job_index)
        num_assets_per_job[job_index] += 1;

    for (uint32_t job_index = 0; job_index < num_jobs_to_dispatch; ++job_index)
    {
        const uint32_t num_assets_for_this_job = num_assets_per_job[job_index];

        const uint32_t job_in_progress_slot = [&]() {
            std::lock_guard lock{m_load_job_record_pool_mutex};

            const uint32_t slot = m_load_job_record_pool_available_indices.front();
            m_load_job_record_pool_available_indices.pop();

            return slot;
        }();

        const uint32_t jobs_in_progress_start = job_in_progress_slot * MAX_ASSETS_PER_LOAD_JOB;

        for (uint32_t asset_index = 0; asset_index < num_assets_for_this_job; ++asset_index)
        {
            LoadJobRecord load_job_record_index{};
            if (!m_load_job_queue.pop(load_job_record_index))
            {
                MIZU_ASSERT(false, "Failed to pop from load jobs queue while dispatching jobs");
                return;
            }

            m_load_job_record_pool[jobs_in_progress_start + asset_index] = load_job_record_index;
        }

        m_load_jobs_in_flight.fetch_add(1, std::memory_order_release);
        g_job_system
            ->schedule(&AssetLoadScheduler::asset_load_job, this, jobs_in_progress_start, num_assets_for_this_job)
            .name("AssetLoadScheduler::AssetLoadJob")
            .submit();
    }
}

void AssetLoadScheduler::request_mesh_load(
    MeshAssetHandle handle,
    uint32_t first_lod,
    uint32_t lod_count,
    std::span<uint8_t> destination,
    const MeshLoadingFinishedFunc& callback)
{
    m_load_job_queue.push({
        .asset_handle = handle,
        .level_base = first_lod,
        .level_count = lod_count,
        .destination = destination,
        .callback_func = [=](AssetKey asset_key) { callback(to_asset_handle<MeshAssetHandle>(asset_key)); },
    });
}

void AssetLoadScheduler::request_texture_load(
    TextureAssetHandle handle,
    uint32_t first_mip,
    uint32_t mip_count,
    std::span<uint8_t> destination,
    const TextureLoadingFinishedFunc& callback)
{
    m_load_job_queue.push({
        .asset_handle = handle,
        .level_base = first_mip,
        .level_count = mip_count,
        .destination = destination,
        .callback_func = [=](AssetKey asset_key) { callback(to_asset_handle<TextureAssetHandle>(asset_key)); },
    });
}

uint32_t AssetLoadScheduler::get_load_jobs_in_flight() const
{
    return m_load_jobs_in_flight.load(std::memory_order_relaxed);
}

void AssetLoadScheduler::asset_load_job(uint32_t job_record_start_index, uint32_t num_assets)
{
    MIZU_PROFILE_SCOPED;

    for (uint32_t i = job_record_start_index; i < job_record_start_index + num_assets; ++i)
    {
        const LoadJobRecord& job_record = m_load_job_record_pool[i];

        const bool loaded =
            std::visit([&](const auto& handle) { return load_asset(handle, job_record); }, job_record.asset_handle);
        if (!loaded)
        {
            MIZU_UNREACHABLE("Failed to load asset");
            continue;
        }

        m_load_job_record_pool[i] = LoadJobRecord{};
    }

    {
        std::lock_guard lock{m_load_job_record_pool_mutex};
        m_load_job_record_pool_available_indices.push(job_record_start_index / MAX_ASSETS_PER_LOAD_JOB);
    }

    m_load_jobs_in_flight.fetch_sub(1, std::memory_order_release);
}

bool AssetLoadScheduler::load_asset(const MeshAssetHandle& handle, const LoadJobRecord& job_record)
{
    MIZU_PROFILE_SCOPED;

    // TODO:
    (void)handle;
    (void)job_record;

    return false;
}

bool AssetLoadScheduler::load_asset(const TextureAssetHandle& handle, const LoadJobRecord& job_record)
{
    MIZU_PROFILE_SCOPED;

    const std::optional<TextureAssetRecord> record = m_asset_loader.get_texture_record(handle);
    if (!record.has_value())
    {
        MIZU_LOG_ERROR("Failed to get texture record for handle: {}", handle.get_id());
        return false;
    }

    if (!m_asset_loader.load_texture_payload_mips(
            handle, job_record.level_base, job_record.level_count, job_record.destination))
    {
        MIZU_LOG_ERROR("Failed to load texture payload for handle: {}", handle.get_id());
        return false;
    }

    job_record.callback_func(to_asset_key(handle));

    return true;
}

} // namespace Mizu
