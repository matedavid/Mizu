#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class CompareImagesShaderCs : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/RenderTestShaders/compare_images.slang",
        Mizu::ShaderType::Compute,
        "cs_compare_images");
};
