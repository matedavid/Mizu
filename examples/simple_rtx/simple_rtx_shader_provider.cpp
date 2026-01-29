#include "shader/shader_registry.h"

#include "copy_shaders.h"
#include "simple_rtx_shaders.h"

using namespace Mizu;

class SimpleRtxShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("SimpleRtxShaders", MIZU_SIMPLE_RTX_SHADERS_SOURCE_PATH);

        registry.register_shader<RaygenShader>();
        registry.register_shader<MissShader>();
        registry.register_shader<ShadowMissShader>();
        registry.register_shader<ClosestHitShader>();
        registry.register_shader<CopyTextureVS>();
        registry.register_shader<CopyTextureFS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(SimpleRtxShaderProvider);
