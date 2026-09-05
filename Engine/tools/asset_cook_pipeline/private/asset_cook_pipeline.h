#pragma once

#include <atomic>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "core/job_system/job_system.h"
#include "package/game_package.h"

#include "asset_cooker.h"
#include "batch_pool.h"
#include "free_range_allocator.h"
#include "timestamp_db.h"

namespace Mizu
{

class AssetCookPipeline
{
  public:
    ~AssetCookPipeline();

    bool init(const GamePackage& package);

    int cook();

  private:
    GamePackage m_package{};
    TimestampDb m_timestamp_db{};
    std::filesystem::path m_timestamp_db_path{};

    std::vector<IAssetImporter*> m_importers{};

    std::vector<IRequestSource*> m_request_sources{};
    std::unordered_map<std::string_view, IAssetImporter*> m_extension_to_importer_map{};
    std::unordered_map<AssetType, IAssetCooker*> m_asset_type_to_cooker_map{};

    JobSystem* m_job_system = nullptr;
    std::atomic<uint32_t> m_in_flight_jobs{0};

    static constexpr size_t BATCH_SIZE = 4;

    using ImportBatch = Batch<ImportRequest, BATCH_SIZE>;
    using CookBatch = Batch<CookRequest, BATCH_SIZE>;
    using SinkBatch = Batch<SinkRequest, BATCH_SIZE>;

    BoundedBatchPool<ImportBatch> m_import_pool{};
    BoundedBatchPool<CookBatch> m_cook_pool{};
    BoundedBatchPool<SinkBatch> m_sink_pool{};

    FreeRangeAllocator m_free_range_allocator{};

    void import_job(ImportBatch* batch);
    void cook_job(CookBatch* batch);
    void sink_job(SinkBatch* batch);

    void dispatch_import_batch(ImportBatch* batch);
    void dispatch_cook_batch(CookBatch* batch);
    void dispatch_sink_batch(SinkBatch* batch);

    IAssetImporter* get_asset_importer(std::string_view extension) const;
    IAssetCooker* get_asset_cooker(AssetType type) const;

    void add_request_source(IRequestSource* source);
    void add_asset_importer(IAssetImporter* importer);
    void add_asset_cooker(IAssetCooker* cooker);
};

} // namespace Mizu