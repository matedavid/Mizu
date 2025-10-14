#include "shader_registry.h"

#include "renderer/shader/shader_declaration.h"

namespace Mizu
{

ShaderRegistry& ShaderRegistry::get()
{
    static ShaderRegistry registry;
    return registry;
}

void ShaderRegistry::register_shader(const ShaderDeclarationMetadata& metadata)
{
    m_shader_metadata_list.push_back(metadata);
}

std::span<const ShaderDeclarationMetadata> ShaderRegistry::get_shader_metadata_list() const
{
    return std::span(m_shader_metadata_list);
}

ShaderRegistryCallback::ShaderRegistryCallback(const ShaderDeclarationMetadata& metadata)
{
    ShaderRegistry::get().register_shader(metadata);
}

} // namespace Mizu