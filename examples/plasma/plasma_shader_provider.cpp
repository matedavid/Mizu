#include "shader/shader_registry.h"

#include "plasma_shaders.h"

using namespace Mizu;

class PlasmaShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("PlasmaShaders", MIZU_PLASMA_SHADERS_SOURCE_PATH);

        registry.register_shader<TextureShaderVS>();
        registry.register_shader<TextureShaderFS>();
        registry.register_shader<ComputeShaderCS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(PlasmaShaderProvider);
