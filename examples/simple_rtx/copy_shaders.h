#pragma once

#include "shader/shader_declaration.h"

using namespace Mizu;

class CopyTextureVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/CopyShaders.slang", ShaderType::Vertex, "copyTextureVertex");
};

class CopyTextureFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/CopyShaders.slang", ShaderType::Fragment, "copyTextureFragment");
};
