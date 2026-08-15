#include "shader/shader_registry.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

//
// ShaderMappingTable
//

void ShaderMappingTable::add_shader_mapping(std::string source, std::string dest)
{
    if (m_mapping_map.contains(source))
    {
        MIZU_LOG_WARNING("Mapping with source '{}' already has a destination", source);
        return;
    }

    m_mapping_map.insert({source, dest});
}

std::optional<std::filesystem::path> ShaderMappingTable::resolve(std::string_view virtual_path) const
{
    /* TODO: Use it when the change to the new virtual format group:path is made
    *
    const size_t pos = virtual_path.find(":");
    if (pos == std::string_view::npos)
    {
        MIZU_ASSERT(false, "Shader virtual path does not follow the format 'group:path'");
        return std::nullopt;
    }

    const std::string_view group = virtual_path.substr(0, pos);
    const std::string_view path = virtual_path.substr(pos + 1);

    const auto it = m_mapping_map.find(std::string{group});
    if (it == m_mapping_map.end())
        return std::nullopt;

    return std::filesystem::path{it->second} / path;
    */

    for (const auto& [source, dest] : m_mapping_map)
    {
        const auto resolved_opt = [&]() -> std::optional<std::filesystem::path> {
            const size_t pos = virtual_path.find(source);
            if (pos == std::string_view::npos)
            {
                return {};
            }

            std::filesystem::path resolved;
            std::string_view rest_of_path = virtual_path.substr(pos + source.size());
            if (rest_of_path.starts_with("/"))
            {
                // Could cause problems because it would be treated as an absolute path
                rest_of_path = rest_of_path.substr(1);
            }

            return std::filesystem::path(dest) / rest_of_path;
        }();

        if (resolved_opt.has_value())
        {
            return resolved_opt;
        }
    }

    return {};
}

//
// ShaderRegistry
//

void ShaderRegistry::register_shader(const ShaderDeclarationMetadata& metadata)
{
    m_shader_metadata_list.push_back(metadata);
}

std::span<const ShaderDeclarationMetadata> ShaderRegistry::get_shader_metadata_list() const
{
    return std::span(m_shader_metadata_list);
}

void ShaderRegistry::add_shader_mapping(std::string source, std::string dest)
{
    m_shader_mapping_table.add_shader_mapping(source, dest);
}

const std::unordered_map<std::string, std::string>& ShaderRegistry::get_shader_mappings() const
{
    return m_shader_mapping_table.get_shader_mappings();
}

//
// ShaderProviderRegistry
//

ShaderProviderRegistry& ShaderProviderRegistry::get()
{
    static ShaderProviderRegistry instance{};
    return instance;
}

ShaderProviderRegistry::~ShaderProviderRegistry()
{
    for (IShaderProvider* provider : m_shader_providers)
    {
        delete provider;
    }
}

} // namespace Mizu