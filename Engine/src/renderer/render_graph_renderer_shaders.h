#pragma once

#include "render_core/render_graph/render_graph_shader_parameters.h"
#include "renderer/shader/shader_declaration.h"

namespace Mizu
{

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseShader_Parameters)
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

class DepthNormalsPrepassShaderVS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3("/EngineShaders/forwardplus/DepthNormalsPrepass.slang", ShaderType::Vertex, "vsMain");
};

class DepthNormalsPrepassShaderFS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3(
        "/EngineShaders/forwardplus/DepthNormalsPrepass.slang",
        ShaderType::Fragment,
        "fsMain");
};

class DepthNormalsPrepassShader
{
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(transformInfo)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(transformIndices)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class LightCullingShaderCS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3("/EngineShaders/forwardplus/LightCulling.slang", ShaderType::Compute, "csMain");

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
        SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(visiblePointLightIndices)
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(lightCullingInfo)
        SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(depthTexture)
        SHADER_PARAMETER_SAMPLER_STATE(depthTextureSampler)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class LightCullingDebugShaderVS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3("/EngineShaders/forwardplus/LightCullingDebug.slang", ShaderType::Vertex, "vsMain");
};

class LightCullingDebugShaderFS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3("/EngineShaders/forwardplus/LightCullingDebug.slang", ShaderType::Fragment, "fsMain");

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", LightCullingShaderCS::TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", LightCullingShaderCS::MAX_LIGHTS_PER_TILE);
    }
};

class LightCullingDebugShader
{
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(visiblePointLightIndices)
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(lightCullingInfo)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class CascadedShadowMappingShaderVS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3(
        "/EngineShaders/forwardplus/CascadedShadowMapping.slang",
        ShaderType::Vertex,
        "vsMain");
};

class CascadedShadowMappingShaderFS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3(
        "/EngineShaders/forwardplus/CascadedShadowMapping.slang",
        ShaderType::Fragment,
        "fsMain");
};

class CascadedShadowMappingShader
{
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(lightSpaceMatrices)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(transformInfo)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(transformIndices)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class CascadedShadowMappingDebugShaderVS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Vertex,
        "vsMain");
};

class CascadedShadowMappingDebugCascadesShaderFS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Fragment,
        "fsCascadesMain");
};

class CascadedShadowMappingDebugTextureShaderFS : public ShaderDeclaration2
{
  public:
    IMPLEMENT_SHADER_DECLARATION3(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Fragment,
        "fsTextureMain");
};

class CascadedShadowMappingDebugCascadesShader
{
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameter, BaseShader_Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(cascadeSplits)
        SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(depthTexture)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class CascadedShadowMappingDebugTextureShader
{
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameter)
        SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(shadowMapTexture)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

// clang-format off
BEGIN_SHADER_PARAMETERS_INHERIT(LightingShaderParameters, BaseShader_Parameters)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(transformInfo)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(transformIndices)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(directionalLights)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(visiblePointLightIndices)
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(lightCullingInfo)
    SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(directionalShadowMap)
    SHADER_PARAMETER_SAMPLER_STATE(directionalShadowMapSampler)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(cascadeSplits)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(lightSpaceMatrices)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

} // namespace Mizu