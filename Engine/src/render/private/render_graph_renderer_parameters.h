#pragma once

#include "render/render_graph/render_graph_shader_parameters.h"

namespace Mizu
{

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseShader_Parameters)
    SHADER_PARAMETER_RG_BUFFER_CBV(cameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

// clang-format off
BEGIN_SHADER_PARAMETERS_INHERIT(DepthNormalsPrepassParameters, BaseShader_Parameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformInfo)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformIndices)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

// clang-format off
BEGIN_SHADER_PARAMETERS_INHERIT(LightCullingShaderParameters, BaseShader_Parameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(pointLights)
    SHADER_PARAMETER_RG_BUFFER_UAV(visiblePointLightIndices)
    SHADER_PARAMETER_RG_BUFFER_CBV(lightCullingInfo)
    SHADER_PARAMETER_RG_TEXTURE_SRV(depthTexture)
    SHADER_PARAMETER_SAMPLER_STATE(depthTextureSampler)
END_SHADER_PARAMETERS()
// clang-format on

// clang-format off
BEGIN_SHADER_PARAMETERS(LightCullingDebugParameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(visiblePointLightIndices)
    SHADER_PARAMETER_RG_BUFFER_CBV(lightCullingInfo)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

// clang-format off
BEGIN_SHADER_PARAMETERS(CascadedShadowMappingParameters)
    SHADER_PARAMETER_RG_BUFFER_SRV(lightSpaceMatrices)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformInfo)
    SHADER_PARAMETER_RG_BUFFER_SRV(transformIndices)
    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

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