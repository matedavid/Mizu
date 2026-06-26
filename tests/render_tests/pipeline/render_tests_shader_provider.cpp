#include "shader/shader_registry.h"

#include "render_tests.pipeline/hello_triangle_shaders.h"

using namespace Mizu;

class RenderShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("RenderTestShaders", RENDER_TESTS_SHADERS_SOURCE_PATH);

        registry.register_shader<HelloTriangleShaderVS>();
        registry.register_shader<HelloTriangleShaderFS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(RenderShaderProvider);
