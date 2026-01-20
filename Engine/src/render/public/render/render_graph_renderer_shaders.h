#pragma once

#include "render/render_graph/render_graph_shader_parameters.h"
#include "render/shader/shader_declaration.h"

namespace Mizu
{

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseShader_Parameters)
    SHADER_PARAMETER_RG_BUFFER_CBV(cameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

class DepthNormalsPrepassShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/DepthNormalsPrepass.slang", ShaderType::Vertex, "vsMain");
};

class DepthNormalsPrepassShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/DepthNormalsPrepass.slang",
        ShaderType::Fragment,
        "fsMain");
};

// clang-format off
BEGIN_SHADER_PARAMETERS_INHERIT(DepthNormalsPrepassParameters, BaseShader_Parameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformInfo)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformIndices)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

class LightCullingShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCulling.slang", ShaderType::Compute, "csMain");

    static constexpr uint32_t TILE_SIZE = 16;
    static constexpr uint32_t MAX_LIGHTS_PER_TILE = 128;

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", MAX_LIGHTS_PER_TILE);
    }

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_BUFFER_SRV(pointLights)
        SHADER_PARAMETER_RG_BUFFER_UAV(visiblePointLightIndices)
        SHADER_PARAMETER_RG_BUFFER_CBV(lightCullingInfo)
        SHADER_PARAMETER_RG_TEXTURE_SRV(depthTexture)
        SHADER_PARAMETER_SAMPLER_STATE(depthTextureSampler)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class LightCullingDebugShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCullingDebug.slang", ShaderType::Vertex, "vsMain");
};

class LightCullingDebugShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCullingDebug.slang", ShaderType::Fragment, "fsMain");

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", LightCullingShaderCS::TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", LightCullingShaderCS::MAX_LIGHTS_PER_TILE);
    }
};

// clang-format off
BEGIN_SHADER_PARAMETERS(LightCullingDebugParameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(visiblePointLightIndices)
    SHADER_PARAMETER_RG_BUFFER_CBV(lightCullingInfo)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

class CascadedShadowMappingShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMapping.slang",
        ShaderType::Vertex,
        "vsMain");
};

class CascadedShadowMappingShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMapping.slang",
        ShaderType::Fragment,
        "fsMain");
};

// clang-format off
BEGIN_SHADER_PARAMETERS(CascadedShadowMappingParameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(lightSpaceMatrices)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformInfo)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformIndices)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

class CascadedShadowMappingDebugShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Vertex,
        "vsMain");
};

class CascadedShadowMappingDebugCascadesShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Fragment,
        "fsCascadesMain");
};

class CascadedShadowMappingDebugTextureShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Fragment,
        "fsTextureMain");
};

// clang-format off
BEGIN_SHADER_PARAMETERS_INHERIT(CascadedShadowMappingDebugCascadesParameters, BaseShader_Parameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(cascadeSplits)
    SHADER_PARAMETER_RG_TEXTURE_SRV(depthTexture)
    SHADER_PARAMETER_SAMPLER_STATE(sampler)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

// clang-format off
BEGIN_SHADER_PARAMETERS(CascadedShadowMappingDebugTextureParameters)
    SHADER_PARAMETER_RG_TEXTURE_SRV(shadowMapTexture)
    SHADER_PARAMETER_SAMPLER_STATE(sampler)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

// clang-format off
BEGIN_SHADER_PARAMETERS_INHERIT(LightingShaderParameters, BaseShader_Parameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformInfo)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformIndices)
    SHADER_PARAMETER_RG_BUFFER_SRV(pointLights)
    SHADER_PARAMETER_RG_BUFFER_SRV(directionalLights)
    SHADER_PARAMETER_RG_BUFFER_SRV(visiblePointLightIndices)
    SHADER_PARAMETER_RG_BUFFER_CBV(lightCullingInfo)
    SHADER_PARAMETER_RG_TEXTURE_SRV(directionalShadowMap)
    SHADER_PARAMETER_SAMPLER_STATE(directionalShadowMapSampler)
    SHADER_PARAMETER_RG_BUFFER_SRV(cascadeSplits)
    SHADER_PARAMETER_RG_BUFFER_SRV(lightSpaceMatrices)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

} // namespace Mizu