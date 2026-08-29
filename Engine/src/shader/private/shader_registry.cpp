#include "shader/shader_registry.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

//
// ShaderMappingTable
//

void ShaderMappingTable::add(std::string source, std::string dest)
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
    m_shader_mapping_table.add(source, dest);
}

std::optional<std::filesystem::path> Mizu::ShaderRegistry::resolve_shader_mapping(std::string_view virtual_path) const
{
    return m_shader_mapping_table.resolve(virtual_path);
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