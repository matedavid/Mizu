#include "shader/shader_registry.h"

#include "hello_triangle_shaders.h"

using namespace Mizu;

class HelloTriangleShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("HelloTriangleShaders", MIZU_HELLO_TRIANGLE_SHADERS_SOURCE_PATH);

        registry.register_shader<HelloTriangleShaderVS>();
        registry.register_shader<HelloTriangleShaderFS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(HelloTriangleShaderProvider);
