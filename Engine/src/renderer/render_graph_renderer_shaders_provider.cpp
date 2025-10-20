#include "renderer/render_graph_renderer_shaders.h"
#include "renderer/shader/shader_registry.h"

using namespace Mizu;

static void register_shaders(ShaderRegistry& registry)
{
    registry.add_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_SOURCE_PATH);

    registry.register_shader<DepthNormalsPrepassShaderVS>();
    registry.register_shader<DepthNormalsPrepassShaderFS>();

    registry.register_shader<LightCullingShaderCS>();

    registry.register_shader<LightCullingDebugShaderVS>();
    registry.register_shader<LightCullingDebugShaderFS>();

    registry.register_shader<CascadedShadowMappingShaderVS>();
    registry.register_shader<CascadedShadowMappingShaderFS>();

    registry.register_shader<CascadedShadowMappingDebugShaderVS>();
    registry.register_shader<CascadedShadowMappingDebugCascadesShaderFS>();
    registry.register_shader<CascadedShadowMappingDebugTextureShaderFS>();
}

MIZU_REGISTER_SHADER_PROVIDER(RenderGraphRendererShaders, register_shaders);
