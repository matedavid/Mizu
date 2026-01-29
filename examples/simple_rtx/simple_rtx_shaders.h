#pragma once

#include "shader/shader_declaration.h"

using namespace Mizu;

class RaygenShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/SimpleRtxShaders.slang", ShaderType::RtxRaygen, "rtxRaygen");
};

class MissShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/SimpleRtxShaders.slang", ShaderType::RtxMiss, "rtxMiss");
};

class ShadowMissShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/SimpleRtxShaders.slang", ShaderType::RtxMiss, "rtxShadowMiss");
};

class ClosestHitShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/SimpleRtxShaders/SimpleRtxShaders.slang", ShaderType::RtxClosestHit, "rtxClosestHit");
};

