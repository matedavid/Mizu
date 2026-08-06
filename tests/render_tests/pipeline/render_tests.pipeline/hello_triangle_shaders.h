#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class HelloTriangleShaderVS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/hello_triangle.slang", Mizu::ShaderType::Vertex, "vs_main");
};

class HelloTriangleShaderFS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/hello_triangle.slang", Mizu::ShaderType::Fragment, "fs_main");
};
