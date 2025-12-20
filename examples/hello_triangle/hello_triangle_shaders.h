#pragma once

#include "render/render_graph/render_graph_shader_parameters.h"
#include "render/shader/shader_declaration.h"

class HelloTriangleShaderVS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/HelloTriangleShaders/HelloTriangle.slang", Mizu::ShaderType::Vertex, "vsMain");
};

class HelloTriangleShaderFS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/HelloTriangleShaders/HelloTriangle.slang", Mizu::ShaderType::Fragment, "fsMain");
};
