#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class PlasmaShaderVS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/plasma.slang", Mizu::ShaderType::Vertex, "vs_main");
};

class PlasmaShaderFS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/plasma.slang", Mizu::ShaderType::Fragment, "fs_main");
};

class PlasmaShaderCS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/plasma.slang", Mizu::ShaderType::Compute, "cs_main");
};
