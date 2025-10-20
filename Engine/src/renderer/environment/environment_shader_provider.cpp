#include "renderer/environment/environment.cpp"
#include "renderer/shader/shader_registry.h"

using namespace Mizu;

static void register_shaders(ShaderRegistry& registry)
{
    registry.register_shader<IrradianceConvolutionShaderVS>();
    registry.register_shader<IrradianceConvolutionShaderFS>();

    registry.register_shader<PrefilterEnvironmentShaderVS>();
    registry.register_shader<PrefilterEnvironmentShaderFS>();

    registry.register_shader<PrecomputeBRDFShaderCS>();
}

MIZU_REGISTER_SHADER_PROVIDER(EnvironmentShaders, register_shaders);