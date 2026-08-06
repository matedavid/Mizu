#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

class DepthPrepassShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/scene_renderer/depth_prepass.slang", ShaderType::Vertex, "vs_main");
};

class DepthPrepassShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/scene_renderer/depth_prepass.slang", ShaderType::Fragment, "fs_main");
};

class PbrOpaqueMaterialShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/scene_renderer/pbr_opaque_material.slang",
        ShaderType::Vertex,
        "vs_main");
};

class PbrOpaqueMaterialShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/scene_renderer/pbr_opaque_material.slang",
        ShaderType::Fragment,
        "fs_main");
};

class LightCullingShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/scene_renderer/light_culling.slang", ShaderType::Compute, "cs_main");

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
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/scene_renderer/lighting.slang", ShaderType::Compute, "cs_main");

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
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/scene_renderer/tonemapping.slang", ShaderType::Vertex, "vs_main");
};

class TonemappingFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/scene_renderer/tonemapping.slang", ShaderType::Fragment, "fs_main");
};

class CascadedShadowMappingVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/scene_renderer/cascaded_shadow_mapping.slang",
        ShaderType::Vertex,
        "vs_main");
};

class CascadedShadowMappingFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/scene_renderer/cascaded_shadow_mapping.slang",
        ShaderType::Fragment,
        "fs_main");
};

} // namespace Mizu
