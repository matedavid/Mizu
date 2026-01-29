#pragma once

#include "shader/shader_declaration.h"

class TextureShaderVS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/PlasmaShaders/TextureShader.slang", Mizu::ShaderType::Vertex, "vsMain");
};

class TextureShaderFS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/PlasmaShaders/TextureShader.slang", Mizu::ShaderType::Fragment, "fsMain");
};

class ComputeShaderCS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/PlasmaShaders/PlasmaShader.slang", Mizu::ShaderType::Compute, "csMain");
};
