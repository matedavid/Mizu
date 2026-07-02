#include "shader/shader_registry.h"

#include "render_tests.pipeline/hello_triangle_shaders.h"
#include "render_tests.pipeline/render_test_shaders.h"

using namespace Mizu;

class RenderShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("RenderTestShaders", RENDER_TESTS_SHADERS_SOURCE_PATH);

        register_render_test_shaders(registry);

        register_hello_triangle_shaders(registry);
    }

  private:
    void register_render_test_shaders(ShaderRegistry& registry) const
    {
        registry.register_shader<CompareImagesShaderCs>();
    }

    void register_hello_triangle_shaders(ShaderRegistry& registry) const
    {
        registry.register_shader<HelloTriangleShaderVS>();
        registry.register_shader<HelloTriangleShaderFS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(RenderShaderProvider);
