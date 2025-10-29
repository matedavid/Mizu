#pragma once

#include "render_core/render_graph/render_graph_shader_parameters.h"
#include "renderer/shader/shader_declaration.h"

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseParameters)
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(uCameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

class TextureShaderVS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/PlasmaExampleShaders/TextureShader.slang", Mizu::ShaderType::Vertex, "vsMain");
};

class TextureShaderFS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/PlasmaExampleShaders/TextureShader.slang", Mizu::ShaderType::Fragment, "fsMain");
};

// clang-format off
BEGIN_SHADER_PARAMETERS_INHERIT(TextureShaderParameters, BaseParameters)
    SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(uTexture)
    SHADER_PARAMETER_SAMPLER_STATE(uTexture_Sampler)

    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

class ComputeShaderCS : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/PlasmaExampleShaders/PlasmaShader.slang", Mizu::ShaderType::Compute, "csMain");

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(uOutput)
    END_SHADER_PARAMETERS()
    // clang-format on
};
