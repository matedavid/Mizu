#include "shader/shader_registry.h"

namespace Mizu
{

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
    const auto it = m_shader_mapping_map.find(source);
    if (it == m_shader_mapping_map.end())
    {
        m_shader_mapping_map.emplace(std::move(source), std::move(dest));
    }
    else
    {
        MIZU_ASSERT(
            it->second == dest,
            "Already registered shader mapping does not match destination path ({} != {})",
            it->second,
            dest);
    }
}

const std::unordered_map<std::string, std::string>& ShaderRegistry::get_shader_mappings() const
{
    return m_shader_mapping_map;
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