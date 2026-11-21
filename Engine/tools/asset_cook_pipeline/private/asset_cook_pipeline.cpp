#include "asset_cook_pipeline.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <thread>

#include "base/debug/profiling.h"
#include "base/reflection/enum_traits.h"

#include "assimp_importer.h"
#include "builtin_assets_cooker.h"
#include "filesystem_request_source.h"
#include "material_cooker.h"
#include "mesh_cooker.h"
#include "prefab_cooker.h"
#include "shader_declaration_cooker.h"
#include "texture_cooker.h"

namespace Mizu
{

AssetCookPipeline::~AssetCookPipeline()
{
    for (IRequestSource* source : m_request_sources)
    {
        delete source;
    }

    // Iterate m_importers instead of m_extension_to_importer_map because, as m_extension_to_importer_map can have the
    // same importer for multiple keys, we can not just iterate and delete.
    for (IAssetImporter* importer : m_importers)
    {
        delete importer;
    }

    for (auto& [_, cooker] : m_asset_type_to_cooker_map)
    {
        delete cooker;
    }
}

bool AssetCookPipeline::init(const GamePackage& package, const CookConfig& config)
{
    MIZU_LOG_SETUP;

    MIZU_PROFILE_SCOPED;

    m_package = package;
    m_config = config;

    if (m_config.num_threads == 0)
        m_config.num_threads = std::thread::hardware_concurrency();
    else
        m_config.num_threads = std::min(std::max(1u, m_config.num_threads), std::thread::hardware_concurrency());

    const uint8_t log_level = static_cast<uint8_t>(m_config.log_level);
    m_reporter.set_settings({
        .log_info = log_level >= static_cast<uint8_t>(LogLevel::Info),
        .log_warning = log_level >= static_cast<uint8_t>(LogLevel::Warning),
        .log_error = log_level >= static_cast<uint8_t>(LogLevel::Error),
    });

    m_job_system = new JobSystem{};

    m_in_flight_jobs.store(0, std::memory_order_relaxed);
    m_outstanding_requests.store(0, std::memory_order_relaxed);

    m_timestamp_db_path = m_package.cook_output_path / "timestamp.db";
    if (std::filesystem::exists(m_timestamp_db_path))
    {
        m_timestamp_db.load(m_timestamp_db_path);
    }
    m_timestamp_db.set_force_different(m_config.force_cook);

    {
        add_request_source(new FilesystemRequestSource{});
        add_request_source(new ShaderDeclarationRequestSource{});
        add_request_source(new BuiltinAssetsRequestSource{});
    }

    {
        add_asset_importer(new AssimpImporter{});
        add_asset_importer(new TextureImporter{});
        add_asset_importer(new ShaderDeclarationImporter{});
        add_asset_importer(new BuiltinTextureImporter{});
    }

    {
        add_asset_cooker(new MeshCooker{});
        add_asset_cooker(new TextureCooker{});
        add_asset_cooker(new MaterialCooker{});
        add_asset_cooker(new PrefabCooker{});
        add_asset_cooker(new ShaderDeclarationCooker{});
    }

    return true;
}

int AssetCookPipeline::cook()
{
    MIZU_PROFILE_SCOPED;

    const uint32_t num_threads = m_config.num_threads;
    if (!m_job_system->init(num_threads, false))
    {
        MIZU_LOG_ERROR("Failed to initialize JobSystem");
        return 1;
    }

    constexpr uint32_t RESERVED_JOBS = 4;
    const uint32_t max_jobs = m_job_system->get_max_in_flight_jobs();
    const uint32_t job_budget = max_jobs > RESERVED_JOBS ? max_jobs - RESERVED_JOBS : 1;

    const uint32_t sink_slots = std::clamp(num_threads / 2, 2u, 4u);
    const uint32_t remaining_slots = job_budget > sink_slots ? job_budget - sink_slots : 2u;
    const uint32_t import_slots = std::max(1u, std::min(num_threads, remaining_slots / 2));
    const uint32_t cook_slots = std::max(1u, std::min(num_threads * 2, remaining_slots - import_slots));

    if (!m_import_stage.init(import_slots))
    {
        MIZU_LOG_ERROR("Failed to initialize ImportStage");
        return 1;
    }

    if (!m_cook_stage.init(cook_slots))
    {
        MIZU_LOG_ERROR("Failed to initialize CookStage");
        return 1;
    }

    if (!m_sink_stage.init(sink_slots))
    {
        MIZU_LOG_ERROR("Failed to initialize SinkStage");
        return 1;
    }

    constexpr size_t allocator_size = 2ull * 1024 * 1024 * 1024; // 2 GB

    if (!m_free_range_allocator.init(allocator_size))
    {
        MIZU_LOG_ERROR("Failed to initialize FreeRangeAllocator");
        return 1;
    }

    m_import_headroom_bytes = allocator_size / 4;

    MIZU_LOG_INFO("AssetCookPipeline for {}", m_package.name);
    MIZU_LOG_INFO("\tNum threads: {}", num_threads);
    MIZU_LOG_INFO("\tForce cook: {}", m_config.force_cook);
    MIZU_LOG_INFO("");

    m_start_time = std::chrono::system_clock::now();

    const CookContext cook_context{
        .asset_mounts = m_package.asset_mounts,
        .timestamp_db = m_timestamp_db,
        .allocator = m_free_range_allocator,
        .reporter = m_reporter,
        .force_cook = m_config.force_cook,
    };

    for (IRequestSource* request_source : m_request_sources)
    {
        if (!request_source->init(cook_context))
        {
            MIZU_ASSERT(false, "Failed to initialize RequestSource");
            return 1;
        }
    }

    constexpr uint32_t ENUMERATE_COUNT = 4 * BATCH_SIZE;

    std::vector<ImportRequest> requests{};

    for (IRequestSource* request_source : m_request_sources)
    {
        bool exhausted = false;
        while (!exhausted)
        {
            const EnumerateResult result = request_source->enumerate_n(ENUMERATE_COUNT, cook_context, requests);
            exhausted = result.exhausted;

            MIZU_ASSERT(
                result.count == requests.size(),
                "RequestSource reported {} enumerated requests but returned {}",
                result.count,
                requests.size());

            const uint32_t num_requests = static_cast<uint32_t>(requests.size());

            m_outstanding_requests.fetch_add(num_requests, std::memory_order_relaxed);
            m_stats.total_import_jobs.fetch_add(num_requests, std::memory_order_relaxed);

            for (ImportRequest& request : requests)
            {
                m_import_stage.push(std::move(request));
            }

            requests.clear();

            pump();
        }
    }

    wait_until_idle();

    MIZU_ASSERT(
        m_import_stage.backlog() == 0 && m_cook_stage.backlog() == 0 && m_sink_stage.backlog() == 0,
        "Pipeline went idle with requests still queued");

    m_job_system->kill();
    m_job_system->wait_workers_dead();

    static constexpr uint32_t MAX_UNUSED_RUNS = 10;
    m_timestamp_db.finalize_run(MAX_UNUSED_RUNS);

    const auto end = std::chrono::system_clock::now();
    const std::chrono::duration<double> elapsed_seconds = end - m_start_time;

    MIZU_LOG_INFO("===================================");
    print_results();

    if (!m_reporter.has_errors())
    {
        MIZU_LOG_INFO("");
        MIZU_LOG_INFO("Summary:");
        MIZU_LOG_INFO("\tTime elapsed: {:.1f}s", elapsed_seconds.count());
        MIZU_LOG_INFO("\tTotal import requests: {}", m_stats.total_import_jobs.load(std::memory_order_relaxed));
        MIZU_LOG_INFO("\tTotal cook requests: {}", m_stats.total_cook_jobs.load(std::memory_order_relaxed));
        MIZU_LOG_INFO("\tAsset completions:");
        for (const auto& [enum_value, completions] : m_reporter.get_asset_completions())
        {
            MIZU_LOG_INFO("\t\t{}: {}", meta::enum_name(enum_value), completions);
        }
    }

    if (!m_timestamp_db.save(m_timestamp_db_path))
    {
        MIZU_LOG_ERROR("Failed to save TimestampDb to {}", m_timestamp_db_path.string());
    }

    return m_reporter.has_errors() ? 1 : 0;
}

void AssetCookPipeline::pump()
{
    // Prioritize sink because cook requests hold memory
    while (SinkBatch* batch = m_sink_stage.try_take_batch())
    {
        dispatch_sink_batch(batch);
    }

    while (CookBatch* batch = m_cook_stage.try_take_batch())
    {
        dispatch_cook_batch(batch);
    }

    while (has_import_headroom())
    {
        ImportBatch* batch = m_import_stage.try_take_batch();
        if (batch == nullptr)
            break;

        dispatch_import_batch(batch);
    }
}

bool AssetCookPipeline::has_import_headroom() const
{
    const size_t reserved_bytes =
        (static_cast<size_t>(m_in_flight_imports.load(std::memory_order_acquire)) + 1) * m_import_headroom_bytes;
    if (m_free_range_allocator.bytes_free() >= reserved_bytes)
        return true;

    return m_in_flight_jobs.load(std::memory_order_acquire) == 0;
}

void AssetCookPipeline::wait_until_idle()
{
    std::unique_lock lock(m_idle_mutex);

    while (m_outstanding_requests.load(std::memory_order_acquire) > 0)
    {
        m_idle_cv.wait_for(lock, std::chrono::seconds(PROGRESS_INTERVAL_SECONDS));

        if (m_outstanding_requests.load(std::memory_order_acquire) > 0)
        {
            print_progress();
        }
    }
}

void AssetCookPipeline::complete_requests(uint32_t count)
{
    const uint32_t previous = m_outstanding_requests.fetch_sub(count, std::memory_order_acq_rel);
    MIZU_ASSERT(previous >= count, "More requests completed than were counted");

    if (previous == count)
    {
        // Take the lock so the notification can't slip in between the waiter's check and its wait
        std::lock_guard lock(m_idle_mutex);
        m_idle_cv.notify_all();
    }
}

void AssetCookPipeline::print_progress() const
{
#if MIZU_DEBUG
    const uint32_t in_flight_jobs = m_in_flight_jobs.load(std::memory_order_acquire);

    const uint32_t finished_import_jobs = m_stats.finished_import_jobs.load(std::memory_order_relaxed);
    const uint32_t finished_cook_jobs = m_stats.finished_cook_jobs.load(std::memory_order_relaxed);

    const uint32_t total_import_jobs = m_stats.total_import_jobs.load(std::memory_order_relaxed);
    const uint32_t total_cook_jobs = m_stats.total_cook_jobs.load(std::memory_order_relaxed);

    const uint32_t remaining_import_jobs = total_import_jobs - finished_import_jobs;
    const uint32_t remaining_cook_jobs = total_cook_jobs - finished_cook_jobs;

    const std::chrono::duration<double> elapsed_seconds = std::chrono::system_clock::now() - m_start_time;

    MIZU_LOG_INFO(
        "In-Flight: {:>3} | Import: {:>5} done ({:>5} left) | Cook: {:>5} done ({:>5} left) | Elapsed: {:>7.1f}s",
        in_flight_jobs,
        finished_import_jobs,
        remaining_import_jobs,
        finished_cook_jobs,
        remaining_cook_jobs,
        elapsed_seconds.count());
#endif
}

void AssetCookPipeline::print_results() const
{
    const std::span<const std::string> infos = m_reporter.get_infos();
    const std::span<const std::string> warnings = m_reporter.get_warnings();

    for (const std::string& info : infos)
    {
        MIZU_LOG_INFO("{}", info);
    }

    for (const std::string& warning : warnings)
    {
        MIZU_LOG_WARNING("{}", warning);
    }

    if (m_reporter.has_errors())
    {
        const std::span<const std::string> errors = m_reporter.get_errors();

        MIZU_LOG_ERROR("Pipeline failed with errors:");
        MIZU_LOG_ERROR("============================");

        for (const std::string& error : errors)
        {
            MIZU_LOG_ERROR("{}", error);
        }
    }
}

void AssetCookPipeline::import_job(ImportBatch* batch)
{
    MIZU_PROFILE_SCOPED;

    const CookContext cook_context{
        .asset_mounts = m_package.asset_mounts,
        .timestamp_db = m_timestamp_db,
        .allocator = m_free_range_allocator,
        .reporter = m_reporter,
    };

    std::vector<CookRequest> cook_requests{};

    for (const ImportRequest& request : batch->get_span())
    {
        IAssetImporter* importer = get_asset_importer(request.extension);
        if (importer == nullptr)
            continue;

        if (!importer->should_import(request, m_timestamp_db))
            continue;

        cook_requests.clear();
        importer->import(request, cook_context, cook_requests);

        const uint32_t num_cook_requests = static_cast<uint32_t>(cook_requests.size());
        m_outstanding_requests.fetch_add(num_cook_requests, std::memory_order_relaxed);
        m_stats.total_cook_jobs.fetch_add(num_cook_requests, std::memory_order_relaxed);

        for (CookRequest& cook_request : cook_requests)
        {
            m_cook_stage.push(std::move(cook_request));
        }
    }

    const uint32_t batch_size = static_cast<uint32_t>(batch->get_span().size());
    m_stats.finished_import_jobs.fetch_add(batch_size, std::memory_order_relaxed);

    m_import_stage.release(batch);
    m_in_flight_imports.fetch_sub(1, std::memory_order_release);
    m_in_flight_jobs.fetch_sub(1, std::memory_order_release);

    pump();

    complete_requests(batch_size);
}

void AssetCookPipeline::cook_job(CookBatch* batch)
{
    MIZU_PROFILE_SCOPED;

    const CookContext cook_context{
        .asset_mounts = m_package.asset_mounts,
        .timestamp_db = m_timestamp_db,
        .allocator = m_free_range_allocator,
        .reporter = m_reporter,
    };

    std::vector<SinkRequest> sink_requests{};

    for (const CookRequest& request : batch->get_span())
    {
        IAssetCooker* cooker = get_asset_cooker(request.asset_type);
        if (cooker == nullptr)
        {
            MIZU_LOG_ERROR("Failed to find asset cooker for asset type: {}", meta::enum_name(request.asset_type));
            continue;
        }

        if (!cooker->should_cook(request, m_timestamp_db))
            continue;

        sink_requests.clear();
        cooker->cook(request, cook_context, sink_requests);

        m_reporter.report_completion(request.asset_type);

        m_outstanding_requests.fetch_add(static_cast<uint32_t>(sink_requests.size()), std::memory_order_relaxed);

        for (SinkRequest& sink_request : sink_requests)
        {
            m_sink_stage.push(std::move(sink_request));
        }
    }

    const uint32_t batch_size = static_cast<uint32_t>(batch->get_span().size());
    m_stats.finished_cook_jobs.fetch_add(batch_size, std::memory_order_relaxed);

    m_cook_stage.release(batch);
    m_in_flight_jobs.fetch_sub(1, std::memory_order_release);

    pump();

    complete_requests(batch_size);
}

void AssetCookPipeline::sink_job(SinkBatch* batch)
{
    MIZU_PROFILE_SCOPED;

    for (const SinkRequest& request : batch->get_span())
    {
        if (request.data.empty())
            continue;

        const std::filesystem::path output_path = m_package.cook_output_path / request.filename;

        if (!std::filesystem::exists(output_path))
        {
            std::filesystem::create_directories(output_path.parent_path());
        }

        std::ofstream file(output_path, std::ios::binary);
        if (!file.is_open())
        {
            std::error_code ec(errno, std::generic_category());
            MIZU_ASSERT(
                false, "Failed to write into file {}: {} (code {})", output_path.string(), ec.message(), ec.value());
            m_free_range_allocator.free(request.data);
            continue;
        }

        file.write(
            reinterpret_cast<const char*>(request.data.data()), static_cast<std::streamsize>(request.data.size()));

        m_free_range_allocator.free(request.data);
    }

    const uint32_t batch_size = static_cast<uint32_t>(batch->get_span().size());

    m_sink_stage.release(batch);
    m_in_flight_jobs.fetch_sub(1, std::memory_order_release);

    pump();

    complete_requests(batch_size);
}

void AssetCookPipeline::dispatch_import_batch(ImportBatch* batch)
{
    m_in_flight_imports.fetch_add(1, std::memory_order_relaxed);
    m_in_flight_jobs.fetch_add(1, std::memory_order_relaxed);
    m_job_system->schedule(&AssetCookPipeline::import_job, this, batch).submit();
}

void AssetCookPipeline::dispatch_cook_batch(CookBatch* batch)
{
    m_in_flight_jobs.fetch_add(1, std::memory_order_relaxed);
    m_job_system->schedule(&AssetCookPipeline::cook_job, this, batch).submit();
}

void AssetCookPipeline::dispatch_sink_batch(SinkBatch* batch)
{
    m_in_flight_jobs.fetch_add(1, std::memory_order_relaxed);
    m_job_system->schedule(&AssetCookPipeline::sink_job, this, batch).submit();
}

IAssetImporter* AssetCookPipeline::get_asset_importer(std::string_view extension) const
{
    const auto it = m_extension_to_importer_map.find(extension);
    return it != m_extension_to_importer_map.end() ? it->second : nullptr;
}

IAssetCooker* AssetCookPipeline::get_asset_cooker(AssetType type) const
{
    const auto it = m_asset_type_to_cooker_map.find(type);
    return it != m_asset_type_to_cooker_map.end() ? it->second : nullptr;
}

void AssetCookPipeline::add_request_source(IRequestSource* source)
{
    m_request_sources.push_back(source);
}

void AssetCookPipeline::add_asset_importer(IAssetImporter* importer)
{
    m_importers.push_back(importer);

    for (std::string_view extension : importer->extensions())
    {
        const auto it = m_extension_to_importer_map.find(extension);
        if (it != m_extension_to_importer_map.end())
        {
            MIZU_LOG_ERROR("Extension '{}' already has an importer registered", extension);
            continue;
        }

        m_extension_to_importer_map.insert({extension, importer});
    }
}

void AssetCookPipeline::add_asset_cooker(IAssetCooker* cooker)
{
    const AssetType type = cooker->asset_type();

    const auto it = m_asset_type_to_cooker_map.find(type);
    if (it != m_asset_type_to_cooker_map.end())
    {
        MIZU_LOG_ERROR("Asset type '{}' already has a cooker registered", meta::enum_name(type));
        return;
    }

    m_asset_type_to_cooker_map.insert({type, cooker});
}

} // namespace Mizu