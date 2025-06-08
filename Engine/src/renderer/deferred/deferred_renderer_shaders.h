#pragma once

#include "render_core/shader/shader_declaration.h"

namespace Mizu
{

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseShader_Parameters)
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

class Deferred_DepthPrePass : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION("/EngineShaders/deferred/DepthPrePass.vert.spv",
                                          "vsMain",
                                          "/EngineShaders/deferred/DepthPrePass.frag.spv",
                                          "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_Shadowmapping : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION("/EngineShaders/deferred/Shadowmapping.vert.spv",
                                          "vsMain",
                                          "/EngineShaders/deferred/Shadowmapping.frag.spv",
                                          "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(lightSpaceMatrices)

        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_PBROpaque : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION("/EngineShaders/deferred/PBROpaque.vert.spv",
                                          "vsMain",
                                          "/EngineShaders/deferred/PBROpaque.frag.spv",
                                          "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_PBRLighting : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION("/EngineShaders/deferred/PBRLighting.vert.spv",
                                          "vsMain",
                                          "/EngineShaders/deferred/PBRLighting.frag.spv",
                                          "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(directionalLights)

        SHADER_PARAMETER_RG_IMAGE_VIEW(directionalShadowmaps)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(directionalLightSpaceMatrices)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(directionalCascadeSplits)

        SHADER_PARAMETER_RG_IMAGE_VIEW(ssaoTexture)

        SHADER_PARAMETER_RG_IMAGE_VIEW(irradianceMap)
        SHADER_PARAMETER_RG_IMAGE_VIEW(prefilteredEnvironmentMap)
        SHADER_PARAMETER_RG_IMAGE_VIEW(precomputedBRDF)

        SHADER_PARAMETER_RG_IMAGE_VIEW(albedo)
        SHADER_PARAMETER_RG_IMAGE_VIEW(normal)
        SHADER_PARAMETER_RG_IMAGE_VIEW(metallicRoughnessAO)
        SHADER_PARAMETER_RG_IMAGE_VIEW(depth)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)

        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_Skybox : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION("/EngineShaders/deferred/Skybox.vert.spv",
                                          "vsMain",
                                          "/EngineShaders/deferred/Skybox.frag.spv",
                                          "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_IMAGE_VIEW(skybox)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)

        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_SSAOMain : public ComputeShaderDeclaration
{
  public:
    IMPLEMENT_COMPUTE_SHADER_DECLARATION("/EngineShaders/deferred/SSAOMain.comp.spv", "ssaoMain");

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(ssaoKernel)
        SHADER_PARAMETER_RG_IMAGE_VIEW(ssaoNoise)
        SHADER_PARAMETER_RG_IMAGE_VIEW(gDepth)
        SHADER_PARAMETER_RG_IMAGE_VIEW(gNormal)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)
        SHADER_PARAMETER_RG_IMAGE_VIEW(mainOutput)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_SSAOBlur : public ComputeShaderDeclaration
{
  public:
    IMPLEMENT_COMPUTE_SHADER_DECLARATION("/EngineShaders/deferred/SSAOBlur.comp.spv", "ssaoBlur");

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_IMAGE_VIEW(blurInput)
        SHADER_PARAMETER_RG_IMAGE_VIEW(blurOutput)
        SHADER_PARAMETER_SAMPLER_STATE(blurSampler)
    END_SHADER_PARAMETERS()
    // clang-format on
};

} // namespace Mizu
