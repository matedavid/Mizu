#include "shader/shader_registry.h"

#include "render.pipeline/scene_renderer_shaders.h"
#include "render.pipeline/scene_shaders.h"

using namespace Mizu;

class RenderShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_SOURCE_PATH);

        register_scene_shaders(registry);
        register_scene_renderer_shaders(registry);
    }

  private:
    void register_scene_shaders(ShaderRegistry& registry) const
    {
        registry.register_shader<PublishTransformsShaderCS>();
    }

    void register_scene_renderer_shaders(ShaderRegistry& registry) const
    {
        registry.register_shader<DepthPrepassShaderVS>();
        registry.register_shader<DepthPrepassShaderFS>();

        registry.register_shader<PbrOpaqueMaterialShaderVS>();
        registry.register_shader<PbrOpaqueMaterialShaderFS>();

        registry.register_shader<LightCullingShaderCS>();
        registry.register_shader<LightingShaderCS>();

        registry.register_shader<TonemappingVS>();
        registry.register_shader<TonemappingFS>();

        registry.register_shader<CascadedShadowMappingVS>();
        registry.register_shader<CascadedShadowMappingFS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(RenderShaderProvider);
