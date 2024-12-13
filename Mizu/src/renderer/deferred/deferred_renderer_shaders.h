#pragma once

#include "render_core/shader/shader_declaration.h"

namespace Mizu

{

class Deferred_BaseShader : public ShaderDeclaration<>
{
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(uCameraInfo)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_DepthPrePass : public ShaderDeclaration<Deferred_BaseShader>
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/DepthPrePass.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/DepthPrePass.frag.spv",
                              "fsMain")

    BEGIN_SHADER_PARAMETERS()
    END_SHADER_PARAMETERS()
};

class Deferred_SimpleColor : public ShaderDeclaration<Deferred_BaseShader>
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/SimpleColor.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/SimpleColor.frag.spv",
                              "fsMain")

    BEGIN_SHADER_PARAMETERS()
    END_SHADER_PARAMETERS()
};

} // namespace Mizu