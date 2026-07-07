#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class SimpleRtxRaygen : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/SimpleRtx.slang", Mizu::ShaderType::RtxRaygen, "rtxRaygen");
};

class SimpleRtxMiss : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/SimpleRtx.slang", Mizu::ShaderType::RtxMiss, "rtxMiss");
};

class SimpleRtxShadowMiss : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/SimpleRtx.slang", Mizu::ShaderType::RtxMiss, "rtxShadowMiss");
};

class SimpleRtxClosestHit : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/RenderTestShaders/SimpleRtx.slang",
        Mizu::ShaderType::RtxClosestHit,
        "rtxClosestHit");
};
