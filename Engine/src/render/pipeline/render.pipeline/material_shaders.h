#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

#include "render_graph_renderer_shaders.h"

namespace Mizu
{

class PBROpaqueShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/PBROpaque.slang", ShaderType::Vertex, "vsMain");
};

class PBROpaqueShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/PBROpaque.slang", ShaderType::Fragment, "fsMain");

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", LightCullingShaderCS::TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", LightCullingShaderCS::MAX_LIGHTS_PER_TILE);
    }
};

} // namespace Mizu
