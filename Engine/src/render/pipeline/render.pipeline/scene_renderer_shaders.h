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

class LightCullingShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/SceneRenderer/LightCulling.slang", ShaderType::Compute, "csMain");

    static constexpr uint32_t TILE_SIZE = 16;
    static constexpr uint32_t MAX_LIGHTS_PER_TILE = 128;

    static void modify_compilation_environment(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", MAX_LIGHTS_PER_TILE);
    }
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

        // Light culling
        environment.set_define("TILE_SIZE", LightCullingShaderCS::TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", LightCullingShaderCS::MAX_LIGHTS_PER_TILE);
    }
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

} // namespace Mizu