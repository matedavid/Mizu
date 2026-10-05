#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

class PublishTransformsShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("engine:scene/publish_transforms.slang", ShaderType::Compute, "cs_main");

    static constexpr uint32_t GROUP_SIZE = 16;

    static void modify_compilation_environment(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("GROUP_SIZE", GROUP_SIZE);
    }
};

class DrawListCullAndGenerateCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "engine:scene/compile_draw_lists.slang",
        ShaderType::Compute,
        "cs_cull_and_generate");

    static constexpr uint32_t GROUP_SIZE = 64;

    static void modify_compilation_environment(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("GROUP_SIZE", GROUP_SIZE);
    }
};

} // namespace Mizu
