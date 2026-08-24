#include "asset_cook_pipeline.h"

#include "base/debug/profiling.h"
#include "base/reflection/enum_traits.h"

#include "assimp_importer.h"
#include "filesystem_request_source.h"
#include "material_cooker.h"
#include "mesh_cooker.h"

namespace Mizu
{

AssetCookPipeline::~AssetCookPipeline()
{
    for (IRequestSource* source : m_request_sources)
    {
        delete source;
    }

    for (auto [_, importer] : m_extension_to_importer_map)
    {
        delete importer;
    }

    for (auto [_, cooker] : m_asset_type_to_cooker_map)
    {
        delete cooker;
    }

    m_request_sources.clear();
    m_extension_to_importer_map.clear();
    m_asset_type_to_cooker_map.clear();
}

bool AssetCookPipeline::init(const GamePackage& package)
{
    MIZU_PROFILE_SCOPED;

    m_package = package;
    m_timestamp_db = TimestampDb{};
    m_job_system = new JobSystem{};

    {
        add_request_source(new FilesystemRequestSource{});
    }

    {
        add_asset_importer(new AssimpImporter{});
    }

    {
        add_asset_cooker(new MeshCooker{});
        add_asset_cooker(new MaterialCooker{});
    }

    return true;
}

int AssetCookPipeline::cook()
{
    MIZU_PROFILE_SCOPED;

    static constexpr uint32_t ENUMERATE_NUMBER = 6;

    const uint32_t num_threads = std::thread::hardware_concurrency();

    if (!m_job_system->init(num_threads - 1, false))
    {
        MIZU_LOG_ERROR("Failed to initialize JobSystem");
        return 1;
    }

    const uint32_t import_batch_count = num_threads * 4;
    const uint32_t cook_batch_count = num_threads * 8;

    if (!m_import_pool.init(import_batch_count))
    {
        MIZU_LOG_ERROR("Failed to initialize Batch ImportPool");
        return 1;
    }

    if (!m_cook_pool.init(cook_batch_count))
    {
        MIZU_LOG_ERROR("Failed to initialize Batch CookPool");
        return 1;
    }

    const CookContext cook_context{
        .asset_mounts = m_package.asset_mounts,
        .timestamp_db = m_timestamp_db,
    };

    for (IRequestSource* request_source : m_request_sources)
    {
        if (!request_source->init(cook_context))
        {
            MIZU_ASSERT(false, "Failed to initialize RequestSource");
            return 1;
        }
    }

    uint32_t request_source_cursor = 0;
    std::vector<ImportRequest> requests{};

    ImportBatch* import_batch = m_import_pool.acquire();

    while (request_source_cursor < m_request_sources.size())
    {
        IRequestSource* request_source = m_request_sources[request_source_cursor];

        const uint32_t enumerated = request_source->enumerate_n(ENUMERATE_NUMBER, requests);
        if (enumerated < ENUMERATE_NUMBER)
        {
            request_source_cursor += 1;
        }

        for (ImportRequest request : requests)
        {
            import_batch->add(std::move(request));

            if (import_batch->is_full())
            {
                m_job_system->schedule(&AssetCookPipeline::import_job, this, import_batch).submit();
                import_batch = m_import_pool.acquire();
            }
        }

        requests.clear();
    }

    // Handle partially filled batch
    if (!import_batch->is_empty())
    {
        m_job_system->schedule(&AssetCookPipeline::import_job, this, import_batch).submit();
    }
    else
    {
        m_import_pool.release(import_batch);
    }

    m_job_system->wait_workers_dead();

    return 0;
}

void AssetCookPipeline::import_job(ImportBatch* batch)
{
    MIZU_PROFILE_SCOPED;

    const CookContext cook_context{
        .asset_mounts = m_package.asset_mounts,
        .timestamp_db = m_timestamp_db,
    };

    CookBatch* cook_batch = m_cook_pool.acquire();

    for (const ImportRequest& request : batch->get_span())
    {
        IAssetImporter* importer = get_asset_importer(request.extension);
        if (importer == nullptr)
            continue;

        if (!importer->should_import(request, m_timestamp_db))
            continue;

        MIZU_LOG_INFO("Importing: {}", request.virtual_path);

        std::vector<CookRequest> cook_requests{};
        importer->import(request, cook_context, cook_requests);

        for (CookRequest& cook_request : cook_requests)
        {
            cook_batch->add(std::move(cook_request));

            if (cook_batch->is_full())
            {
                m_job_system->schedule(&AssetCookPipeline::cook_job, this, cook_batch).submit();
                cook_batch = m_cook_pool.acquire();
            }
        }
    }

    if (!cook_batch->is_empty())
    {
        m_job_system->schedule(&AssetCookPipeline::cook_job, this, cook_batch).submit();
    }
    else
    {
        m_cook_pool.release(cook_batch);
    }

    m_import_pool.release(batch);
}

void AssetCookPipeline::cook_job(CookBatch* batch)
{
    MIZU_PROFILE_SCOPED;

    for (const CookRequest& request : batch->get_span())
    {
        MIZU_LOG_INFO("Cooking: {}", meta::enum_name(request.asset_type));
    }

    m_cook_pool.release(batch);
}

IAssetImporter* AssetCookPipeline::get_asset_importer(std::string_view extension) const
{
    const auto it = m_extension_to_importer_map.find(extension);
    return it != m_extension_to_importer_map.end() ? it->second : nullptr;
}

void AssetCookPipeline::add_request_source(IRequestSource* source)
{
    m_request_sources.push_back(source);
}

void AssetCookPipeline::add_asset_importer(IAssetImporter* importer)
{
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
