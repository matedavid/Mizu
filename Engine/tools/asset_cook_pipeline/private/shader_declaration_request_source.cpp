#include "shader_declaration_request_source.h"

#include "shader_declaration_cooker.h"

namespace Mizu
{

static std::optional<AssetMount> get_shader_declaration_asset_mount(
    std::string_view virtual_path,
    const ShaderMappingTable& table)
{
    const size_t pos = virtual_path.find(":");

    if (pos == std::string_view::npos)
        return std::nullopt;

    const std::string_view mount_name = virtual_path.substr(0, pos);

    const auto& mapping_table = table.get_mapping_map();

    const auto it = mapping_table.find(std::string{mount_name});
    if (it != mapping_table.end())
    {
        return AssetMount{
            .path = it->second,
            .name = std::string{mount_name},
        };
    }

    return std::nullopt;
}

bool ShaderDeclarationRequestSource::init(const CookContext&)
{
    const ShaderProviderRegistry& provider_registry = ShaderProviderRegistry::get();

    for (IShaderProvider* const provider : provider_registry.get_shader_providers())
    {
        provider->register_shaders(m_registry);
    }

    for (const auto& [_, destination] : m_registry.get_shader_mapping_table().get_mapping_map())
    {
        m_include_paths.push_back(destination);
    }

    // Always add the common shader include path
    m_include_paths.push_back(MIZU_ENGINE_SHADERS_SOURCE_PATH);

    m_shader_metadata = m_registry.get_shader_metadata_list();
    m_shader_cursor = 0;

    return true;
}

uint32_t ShaderDeclarationRequestSource::enumerate_n(uint32_t number, std::vector<ImportRequest>& outputs)
{
    uint32_t num_enumerated = 0;

    while (num_enumerated < number && m_shader_cursor < m_shader_metadata.size())
    {
        const ShaderDeclarationMetadata& metadata = m_shader_metadata[m_shader_cursor++];

        const ShaderMappingTable& mapping_table = m_registry.get_shader_mapping_table();
        const std::optional<std::filesystem::path> path = mapping_table.resolve(metadata.virtual_path);

        if (!path.has_value())
        {
            MIZU_ASSERT(false, "Failed to resolve path for shader metadata: {}", metadata.virtual_path);
            continue;
        }

        const std::optional<AssetMount> asset_mount =
            get_shader_declaration_asset_mount(metadata.virtual_path, mapping_table);

        if (!asset_mount.has_value())
        {
            MIZU_ASSERT(false, "Failed to resolve asset mount for shader metadata: {}", metadata.virtual_path);
            continue;
        }

        outputs.push_back({
            .extension = ".slang",
            .path = *path,
            .virtual_path = std::string{metadata.virtual_path},
            .asset_mount = *asset_mount,
            .payload =
                ShaderDeclarationImportPayload{
                    .metadata = metadata,
                    .include_paths = m_include_paths,
                },
        });

        num_enumerated += 1;
    }

    return num_enumerated;
}

} // namespace Mizu