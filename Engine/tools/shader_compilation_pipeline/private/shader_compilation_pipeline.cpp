#include <iostream>

#include "shader/shader_registry.h"

using namespace Mizu;

int main()
{
    const ShaderProviderRegistry& provider_registry = ShaderProviderRegistry::get();

    ShaderRegistry registry{};
    for (IShaderProvider* const provider : provider_registry.get_shader_providers())
    {
        provider->register_shaders(registry);
    }

    for (const ShaderDeclarationMetadata& metadata : registry.get_shader_metadata_list())
    {
        std::cout << "Registered Shader: " << metadata.virtual_path << " (Entry Point: " << metadata.entry_point
                  << ")\n";
    }
}