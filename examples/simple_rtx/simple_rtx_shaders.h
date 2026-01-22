#pragma once

#include "render/render_graph/render_graph_shader_parameters.h"
#include "shader/shader_declaration.h"

using namespace Mizu;

class RaygenShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/Example.slang", ShaderType::RtxRaygen, "rtxRaygen");
};

class MissShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/Example.slang", ShaderType::RtxMiss, "rtxMiss");
};

class ShadowMissShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/Example.slang", ShaderType::RtxMiss, "rtxShadowMiss");
};

class ClosestHitShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/Example.slang", ShaderType::RtxClosestHit, "rtxClosestHit");
};

// clang-format off
BEGIN_SHADER_PARAMETERS(SimpleRtxParameters)
    SHADER_PARAMETER_RG_BUFFER_CBV(cameraInfo)
    SHADER_PARAMETER_RG_TEXTURE_UAV(output)
    SHADER_PARAMETER_RG_ACCELERATION_STRUCTURE(scene)

    SHADER_PARAMETER_RG_BUFFER_SRV(vertices)
    SHADER_PARAMETER_RG_BUFFER_SRV(indices)
    SHADER_PARAMETER_RG_BUFFER_SRV(pointLights)
END_SHADER_PARAMETERS()
// clang-format on
