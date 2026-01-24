#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

class DepthNormalsPrepassShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/DepthNormalsPrepass.slang", ShaderType::Vertex, "vsMain");
};

class DepthNormalsPrepassShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/DepthNormalsPrepass.slang",
        ShaderType::Fragment,
        "fsMain");
};

class LightCullingShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCulling.slang", ShaderType::Compute, "csMain");

    static constexpr uint32_t TILE_SIZE = 16;
    static constexpr uint32_t MAX_LIGHTS_PER_TILE = 128;

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", MAX_LIGHTS_PER_TILE);
    }
};

class LightCullingDebugShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCullingDebug.slang", ShaderType::Vertex, "vsMain");
};

class LightCullingDebugShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/LightCullingDebug.slang", ShaderType::Fragment, "fsMain");

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", LightCullingShaderCS::TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", LightCullingShaderCS::MAX_LIGHTS_PER_TILE);
    }
};

class CascadedShadowMappingShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMapping.slang",
        ShaderType::Vertex,
        "vsMain");
};

class CascadedShadowMappingShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMapping.slang",
        ShaderType::Fragment,
        "fsMain");
};

class CascadedShadowMappingDebugShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Vertex,
        "vsMain");
};

class CascadedShadowMappingDebugCascadesShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Fragment,
        "fsCascadesMain");
};

class CascadedShadowMappingDebugTextureShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/forwardplus/CascadedShadowMappingDebug.slang",
        ShaderType::Fragment,
        "fsTextureMain");
};

} // namespace Mizu