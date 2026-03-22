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

    static void modify_compilation_environment(
        const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        // In targets where NDC y=+1 points toward the top of the screen (low UV-V),
        // the shadow map vertex shader must flip Y so that stored depth values are
        // addressable with the standard (ndc.y * 0.5 + 0.5) UV formula used in the
        // lighting pass. Vulkan's rasterizer inverts Y automatically; other targets
        // (DXIL, Metal, ...) do not, so we compensate here.
        if (target.target != ShaderBytecodeTarget::Spirv)
            environment.set_define("MIZU_NDC_Y_UP", 1);
    }
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