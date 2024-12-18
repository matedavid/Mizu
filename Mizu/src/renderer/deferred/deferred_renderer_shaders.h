#pragma once

#include "render_core/shader/material_shader.h"
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

class Deferred_PBROpaque : public MaterialShader<Deferred_BaseShader>
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/PBROpaque.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/PBROpaque.frag.spv",
                              "fsMain")

    // clang-format off
    BEGIN_MATERIAL_PARAMETERS()
        MATERIAL_PARAMETER_TEXTURE2D(albedo)
        MATERIAL_PARAMETER_TEXTURE2D(metallic)
        MATERIAL_PARAMETER_TEXTURE2D(roughness)
    END_MATERIAL_PARAMETERS()
    // clang-format on
};

class Deferred_PBRLighting : public ShaderDeclaration<>
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/PBRLighting.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/PBRLighting.frag.spv",
                              "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_TEXTURE2D(albedo)
        SHADER_PARAMETER_RG_TEXTURE2D(position)
        SHADER_PARAMETER_RG_TEXTURE2D(normals)
        SHADER_PARAMETER_RG_TEXTURE2D(metallicRoughnessAO)
    END_SHADER_PARAMETERS()
    // clang-format on
};

} // namespace Mizu