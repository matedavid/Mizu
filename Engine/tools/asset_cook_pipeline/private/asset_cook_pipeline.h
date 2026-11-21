#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "core/job_system/job_system.h"
#include "core/package/game_package.h"

#include "asset_cook_reporter.h"
#include "asset_cooker.h"
#include "free_range_allocator.h"
#include "pipeline_stage.h"
#include "timestamp_db.h"

namespace Mizu
{

enum class LogLevel
{
    None = 0,
    Info,
    Warning,
    Error,
};

struct CookConfig
{
    LogLevel log_level = LogLevel::None;
    uint32_t num_threads = 0;
    bool force_cook = false;
};

class AssetCookPipeline
{
  public:
    ~AssetCookPipeline();

    bool init(const GamePackage& package, const CookConfig& config);

    int cook();

  private:
    GamePackage m_package{};
    CookConfig m_config{};
    TimestampDb m_timestamp_db{};
    AssetCookReporter m_reporter{};
    std::filesystem::path m_timestamp_db_path{};

    std::vector<IAssetImporter*> m_importers{};

    std::vector<IRequestSource*> m_request_sources{};
    std::unordered_map<std::string_view, IAssetImporter*> m_extension_to_importer_map{};
    std::unordered_map<AssetType, IAssetCooker*> m_asset_type_to_cooker_map{};

    JobSystem* m_job_system = nullptr;

    std::atomic<uint32_t> m_in_flight_jobs{0};
    std::atomic<uint32_t> m_outstanding_requests{0};

    std::mutex m_idle_mutex{};
    std::condition_variable m_idle_cv{};

    std::chrono::system_clock::time_point m_start_time{};

    struct CookStatistics
    {
        std::atomic<uint32_t> total_import_jobs{0};
        std::atomic<uint32_t> total_cook_jobs{0};

        std::atomic<uint32_t> finished_import_jobs{0};
        std::atomic<uint32_t> finished_cook_jobs{0};
    } m_stats{};

    static constexpr size_t BATCH_SIZE = 4;
    static constexpr uint32_t PROGRESS_INTERVAL_SECONDS = 2;

    using ImportStage = PipelineStage<ImportRequest, BATCH_SIZE>;
    using CookStage = PipelineStage<CookRequest, BATCH_SIZE>;
    using SinkStage = PipelineStage<SinkRequest, BATCH_SIZE>;

    using ImportBatch = ImportStage::BatchType;
    using CookBatch = CookStage::BatchType;
    using SinkBatch = SinkStage::BatchType;

    ImportStage m_import_stage{};
    CookStage m_cook_stage{};
    SinkStage m_sink_stage{};

    FreeRangeAllocator m_free_range_allocator{};

    size_t m_import_headroom_bytes = 0;
    std::atomic<uint32_t> m_in_flight_imports{0};

    void import_job(ImportBatch* batch);
    void cook_job(CookBatch* batch);
    void sink_job(SinkBatch* batch);

    void pump();
    bool has_import_headroom() const;

    void dispatch_import_batch(ImportBatch* batch);
    void dispatch_cook_batch(CookBatch* batch);
    void dispatch_sink_batch(SinkBatch* batch);

    void complete_requests(uint32_t count);
    void wait_until_idle();
    void print_progress() const;

    void print_results() const;

    IAssetImporter* get_asset_importer(std::string_view extension) const;
    IAssetCooker* get_asset_cooker(AssetType type) const;

    void add_request_source(IRequestSource* source);
    void add_asset_importer(IAssetImporter* importer);
    void add_asset_cooker(IAssetCooker* cooker);
};

} // namespace Mizu