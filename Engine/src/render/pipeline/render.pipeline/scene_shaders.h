#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

class PublishTransformsShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/Scene/PublishTransforms.slang", ShaderType::Compute, "csMain");

    static constexpr uint32_t GROUP_SIZE = 16;

    static void modify_compilation_environment(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("GROUP_SIZE", GROUP_SIZE);
    }
};

class DrawListCullInstancesCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/Scene/CompileDrawLists.slang", ShaderType::Compute, "csCullInstances");

    static constexpr uint32_t GROUP_SIZE = 16;

    static void modify_compilation_environment(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("GROUP_SIZE", GROUP_SIZE);
    }
};

class DrawListGenerateCommandsCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/Scene/CompileDrawLists.slang",
        ShaderType::Compute,
        "csGenerateCommands");

    static constexpr uint32_t GROUP_SIZE = 16;

    static void modify_compilation_environment(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("GROUP_SIZE", GROUP_SIZE);
    }
};

} // namespace Mizu