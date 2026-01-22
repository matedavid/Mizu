#include "shader/shader_registry.h"

#include "plasma_shaders.h"

using namespace Mizu;

static void register_shaders(ShaderRegistry& registry)
{
    registry.add_shader_mapping("PlasmaExampleShaders", MIZU_EXAMPLE_PLASMA_SHADERS_SOURCE_PATH);
    registry.add_shader_output_mapping("PlasmaExampleShaders", MIZU_EXAMPLE_PLASMA_SHADERS_OUTPUT_PATH);

    registry.register_shader<TextureShaderVS>();
    registry.register_shader<TextureShaderFS>();
    registry.register_shader<ComputeShaderCS>();
}

MIZU_REGISTER_SHADER_PROVIDER(PlasmaShaders, register_shaders);