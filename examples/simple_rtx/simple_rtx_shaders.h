#pragma once

#include "render_core/render_graph/render_graph_shader_parameters.h"
#include "renderer/shader/shader_declaration.h"

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
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
    SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(output)
    SHADER_PARAMETER_RG_ACCELERATION_STRUCTURE(scene)

    SHADER_PARAMETER_RG_STORAGE_BUFFER(vertices)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(indices)
    SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
END_SHADER_PARAMETERS()
// clang-format on
