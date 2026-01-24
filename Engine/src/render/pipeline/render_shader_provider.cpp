#include "shader/shader_registry.h"

#include "render.pipeline/material_shaders.h"
#include "render.pipeline/render_graph_renderer_shaders.h"

using namespace Mizu;

class RenderShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_SOURCE_PATH);

        register_render_graph_renderer_shaders(registry);
        register_material_shaders(registry);
    }

  private:
    void register_render_graph_renderer_shaders(ShaderRegistry& registry) const
    {
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

    void register_material_shaders(ShaderRegistry& registry) const
    {
        registry.register_shader<PBROpaqueShaderVS>();
        registry.register_shader<PBROpaqueShaderFS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(RenderShaderProvider);
