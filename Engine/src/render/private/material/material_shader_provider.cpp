#include "render/material/material.h"
#include "render/shader/shader_registry.h"

using namespace Mizu;

static void register_shaders(ShaderRegistry& registry)
{
    registry.register_shader<PBROpaqueShaderVS>();
    registry.register_shader<PBROpaqueShaderFS>();
}

MIZU_REGISTER_SHADER_PROVIDER(MaterialShaders, register_shaders);