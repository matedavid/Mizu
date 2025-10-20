#include "renderer/shader/shader_registry.h"

#include "hello_triangle_shaders.h"

using namespace Mizu;

static void register_shaders(ShaderRegistry& registry)
{
    registry.add_shader_mapping("HelloTriangleShaders", MIZU_EXAMPLE_HELLO_TRIANGLE_SHADERS_SOURCE_PATH);
    registry.add_shader_output_mapping("HelloTriangleShaders", MIZU_EXAMPLE_HELLO_TRIANGLE_SHADERS_OUTPUT_PATH);

    registry.register_shader<HelloTriangleShaderVS>();
    registry.register_shader<HelloTriangleShaderFS>();
}

MIZU_REGISTER_SHADER_PROVIDER(HelloTriangleShaders, register_shaders);
