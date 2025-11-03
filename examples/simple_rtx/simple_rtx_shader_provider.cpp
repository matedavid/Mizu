#include "renderer/shader/shader_registry.h"

#include "simple_rtx_shaders.h"

using namespace Mizu;

static void register_shaders(ShaderRegistry& registry)
{
    registry.add_shader_mapping("SimpleRtxShaders", MIZU_EXAMPLE_SIMPLE_RTX_SHADERS_SOURCE_PATH);
    registry.add_shader_output_mapping("SimpleRtxShaders", MIZU_EXAMPLE_SIMPLE_RTX_SHADERS_OUTPUT_PATH);

    registry.register_shader<RaygenShader>();
    registry.register_shader<MissShader>();
    registry.register_shader<ShadowMissShader>();
    registry.register_shader<ClosestHitShader>();
}

MIZU_REGISTER_SHADER_PROVIDER(SimpleRtxShaders, register_shaders);
