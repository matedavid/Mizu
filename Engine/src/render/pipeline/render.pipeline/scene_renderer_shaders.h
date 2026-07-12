#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

class DepthPrepassShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/DepthPrepass.slang", ShaderType::Vertex, "vsMain");
};

class DepthPrepassShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/DepthPrepass.slang", ShaderType::Fragment, "fsMain");
};

} // namespace Mizu