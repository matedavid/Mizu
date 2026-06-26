#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class HelloTriangleShaderVS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/HelloTriangle.slang", Mizu::ShaderType::Vertex, "vsMain");
};

class HelloTriangleShaderFS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/HelloTriangle.slang", Mizu::ShaderType::Fragment, "fsMain");
};
