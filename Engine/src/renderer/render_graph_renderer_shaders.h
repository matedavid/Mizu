#pragma once

#include "render_core/shader/shader_declaration.h"

namespace Mizu
{

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseShader_Parameters)
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

class DepthPrePassShader : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/DepthPrePass.vert.spv",
        "vsMain",
        "/EngineShaders/forwardplus/DepthPrePass.frag.spv",
        "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class LightCullingShader : public ComputeShaderDeclaration
{
  public:
    IMPLEMENT_COMPUTE_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCulling.comp.spv", "csMain");

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
        SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(depthTexture)
        SHADER_PARAMETER_SAMPLER_STATE(depthTextureSampler)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(visiblePointLightIndices)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class LightCullingDebugShader : public ComputeShaderDeclaration
{
  public:
    IMPLEMENT_COMPUTE_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCullingDebug.comp.spv", "csMain");

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(visiblePointLightIndices)
        SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(output)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class SimpleLightingShader : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/SimpleLighting.vert.spv",
        "vsMain",
        "/EngineShaders/forwardplus/SimpleLighting.frag.spv",
        "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(directionalLights)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
};

} // namespace Mizu