#pragma once

#include "renderer/shader_declaration.h"

namespace Mizu {

class ForwardRenderer_BaseShader : public ShaderDeclaration<> {
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(uCameraInfo)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class ForwardRenderer_BasicShader : public ShaderDeclaration<ForwardRenderer_BaseShader> {
  public:
    IMPLEMENT_SHADER("/EngineShaders/forward/basic.vert.spv", "/EngineShaders/forward/basic.frag.spv")

    // clang-format off
    BEGIN_SHADER_PARAMETERS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

} // namespace Mizu