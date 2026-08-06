#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

class SimpleRtxRaygen : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/simple_rtx.slang", Mizu::ShaderType::RtxRaygen, "rtx_raygen");
};

class SimpleRtxMiss : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/simple_rtx.slang", Mizu::ShaderType::RtxMiss, "rtx_miss");
};

class SimpleRtxShadowMiss : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/RenderTestShaders/simple_rtx.slang", Mizu::ShaderType::RtxMiss, "rtx_shadow_miss");
};

class SimpleRtxClosestHit : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/RenderTestShaders/simple_rtx.slang",
        Mizu::ShaderType::RtxClosestHit,
        "rtx_closest_hit");
};
