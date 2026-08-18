#include "asset_cook_pipeline.h"

#include <vector>

#include "base/debug/logging.h"
#include "base/reflection/enum_traits.h"

#include "filesystem_cook_request_source.h"
#include "mesh_asset_cooker.h"
#include "shader_declaration_cook_request_source.h"

namespace Mizu
{

AssetCookPipeline::~AssetCookPipeline()
{
    for (ICookRequestSource* source : m_cook_request_sources)
    {
        delete source;
    }
}

bool AssetCookPipeline::init(const GamePackage& package)
{
    m_package = package;

    {
        FilesystemCookRequestSource* filesystem_source = new FilesystemCookRequestSource{};
        filesystem_source->add_asset_importer<MeshImporter>();

        ShaderDeclarationCookRequestSource* shader_declaration_source = new ShaderDeclarationCookRequestSource{};

        add_cook_request_source(filesystem_source);
        add_cook_request_source(shader_declaration_source);
    }

    {
        add_asset_cooker(new MeshCooker{});
    }

    return true;
}

int AssetCookPipeline::cook()
{
    const CookContext cook_context{
        .asset_mounts = m_package.asset_mounts,
    };

    for (ICookRequestSource* source : m_cook_request_sources)
    {
        source->init(cook_context);
    }

    for (ICookRequestSource* source : m_cook_request_sources)
    {
        std::vector<CookRequest> requests;
        while (source->enumerate_n(cook_context, 4, requests) > 0)
        {
            for (const CookRequest& req : requests)
            {
                const ShaderDeclarationCookInfo* cook_info = std::get_if<ShaderDeclarationCookInfo>(&req.cook_info);
                if (cook_info == nullptr)
                    continue;

                MIZU_LOG_INFO(
                    "Shader declaration: {} - {}",
                    cook_info->path.string(),
                    cook_info->environment.get_shader_defines());
            }

            requests.clear();
        }
    }

    return 0;
}

void AssetCookPipeline::add_cook_request_source(ICookRequestSource* source)
{
    m_cook_request_sources.push_back(source);
}

void AssetCookPipeline::add_asset_cooker(IAssetCooker* cooker)
{
    const AssetCookType asset_type = cooker->asset_type();

    if (m_asset_type_to_cooker_map.contains(asset_type))
    {
        MIZU_LOG_WARNING("Cooker for asset '{}' is already registered", meta::enum_name(asset_type));
        return;
    }

    m_asset_type_to_cooker_map.insert({asset_type, cooker});
}

} // namespace Mizu
