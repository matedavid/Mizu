#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class CompareImagesShaderCs : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/CompareImages.slang", Mizu::ShaderType::Compute, "compareImagesCs");
};
