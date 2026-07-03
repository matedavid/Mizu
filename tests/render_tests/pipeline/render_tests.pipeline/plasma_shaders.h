#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class PlasmaShaderVS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/Plasma.slang", Mizu::ShaderType::Vertex, "vsMain");
};

class PlasmaShaderFS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/Plasma.slang", Mizu::ShaderType::Fragment, "fsMain");
};

class PlasmaShaderCS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/Plasma.slang", Mizu::ShaderType::Compute, "csMain");
};
