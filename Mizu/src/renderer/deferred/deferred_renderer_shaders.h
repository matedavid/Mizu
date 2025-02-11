#pragma once

#include "render_core/shader/shader_declaration.h"

namespace Mizu
{

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseShader_Parameters)
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

class Deferred_DepthPrePass : public ShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/DepthPrePass.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/DepthPrePass.frag.spv",
                              "fsMain")

    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
    END_SHADER_PARAMETERS()
};

class Deferred_Shadowmapping : public ShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/Shadowmapping.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/Shadowmapping.frag.spv",
                              "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(lightViewMatrices)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_PBROpaque : public ShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/PBROpaque.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/PBROpaque.frag.spv",
                              "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_PBRLighting : public ShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/PBRLighting.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/PBRLighting.frag.spv",
                              "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(directionalLights)

        SHADER_PARAMETER_RG_TEXTURE2D(albedo)
        SHADER_PARAMETER_RG_TEXTURE2D(position)
        SHADER_PARAMETER_RG_TEXTURE2D(normal)
        SHADER_PARAMETER_RG_TEXTURE2D(metallicRoughnessAO)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class Deferred_Skybox : public ShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/deferred/Skybox.vert.spv",
                              "vsMain",
                              "/EngineShaders/deferred/Skybox.frag.spv",
                              "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseShader_Parameters)
        SHADER_PARAMETER_RG_CUBEMAP(skybox)
    END_SHADER_PARAMETERS()
    // clang-format on
};

} // namespace Mizu
