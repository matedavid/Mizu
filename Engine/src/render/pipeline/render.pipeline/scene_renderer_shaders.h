#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

class DepthPrepassShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/DepthPrepass.slang", ShaderType::Vertex, "vsMain");
};

class DepthPrepassShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/DepthPrepass.slang", ShaderType::Fragment, "fsMain");
};

class PbrOpaqueMaterialShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/PbrOpaqueMaterial.slang", ShaderType::Vertex, "vsMain");
};

class PbrOpaqueMaterialShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/SceneRenderer/PbrOpaqueMaterial.slang",
        ShaderType::Fragment,
        "fsMain");
};

class TonemappingVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/Tonemapping.slang", ShaderType::Vertex, "vsMain");
};

class TonemappingFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/Tonemapping.slang", ShaderType::Fragment, "fsMain");
};

class LightingShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/Lighting.slang", ShaderType::Compute, "csMain");

    static constexpr uint32_t GROUP_COUNT = 16;

    static void modify_compilation_environment(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("GROUP_COUNT", GROUP_COUNT);
    }
};

} // namespace Mizu