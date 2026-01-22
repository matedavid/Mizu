#pragma once

#include <memory>

#include "mizu_render_module.h"
#include "render/core/image_utils.h"
#include "render/render_graph/render_graph_shader_parameters.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

// Forward declarations
class RenderGraphBuilder;
struct RGTextureSrvRef;

class IrradianceConvolutionShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/environment/IrradianceConvolution.slang",
        ShaderType::Vertex,
        "vsMain");
};

class IrradianceConvolutionShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/environment/IrradianceConvolution.slang",
        ShaderType::Fragment,
        "fsMain");
};

// clang-format off
BEGIN_SHADER_PARAMETERS(IrradianceConvolutionParameters)
    SHADER_PARAMETER_RG_TEXTURE_SRV(environmentMap)
    SHADER_PARAMETER_SAMPLER_STATE(sampler)

    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

class PrefilterEnvironmentShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/environment/PrefilterEnvironment.slang", ShaderType::Vertex, "vsMain");
};

class PrefilterEnvironmentShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/environment/PrefilterEnvironment.slang",
        ShaderType::Fragment,
        "fsMain");
};

// clang-format off
BEGIN_SHADER_PARAMETERS(PrefilterEnvironmentParameters)
    SHADER_PARAMETER_RG_TEXTURE_SRV(environmentMap)
    SHADER_PARAMETER_SAMPLER_STATE(sampler)

    SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
END_SHADER_PARAMETERS()
// clang-format on

class PrecomputeBRDFShaderCS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION(
        "/EngineShaders/environment/PrecomputeBRDF.slang",
        ShaderType::Compute,
        "precomputeBRDF");

    static constexpr uint32_t GROUP_SIZE = 16;

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("GROUP_SIZE", GROUP_SIZE);
    }
};

// clang-format off
BEGIN_SHADER_PARAMETERS(PrecomputeBRDFParameters)
    SHADER_PARAMETER_RG_TEXTURE_UAV(output)
END_SHADER_PARAMETERS()
// clang-format on

class MIZU_RENDER_API Environment
{
  public:
    static std::shared_ptr<Environment> create(const ImageUtils::Faces& faces);
    static std::shared_ptr<Environment> create(std::shared_ptr<ImageResource> cubemap);

    std::shared_ptr<ImageResource> get_cubemap() const { return m_cubemap; }
    std::shared_ptr<ImageResource> get_irradiance_map() const { return m_irradiance_map; }
    std::shared_ptr<ImageResource> get_prefiltered_environment_map() const { return m_prefiltered_environment_map; }
    std::shared_ptr<ImageResource> get_precomputed_brdf() const { return m_precomputed_brdf; }

  private:
    Environment(
        std::shared_ptr<ImageResource> cubemap,
        std::shared_ptr<ImageResource> irradiance_map,
        std::shared_ptr<ImageResource> prefiltered_environment_map,
        std::shared_ptr<ImageResource> precomputed_brdf);

    std::shared_ptr<ImageResource> m_cubemap;

    std::shared_ptr<ImageResource> m_irradiance_map;
    std::shared_ptr<ImageResource> m_prefiltered_environment_map;
    std::shared_ptr<ImageResource> m_precomputed_brdf;

    static std::shared_ptr<Environment> create_internal(std::shared_ptr<ImageResource> cubemap);

    static std::shared_ptr<ImageResource> create_irradiance_map(
        RenderGraphBuilder& builder,
        RGTextureSrvRef cubemap_ref);
    static std::shared_ptr<ImageResource> create_prefiltered_environment_map(
        RenderGraphBuilder& builder,
        RGTextureSrvRef cubemap_ref);
    static std::shared_ptr<ImageResource> create_precomputed_brdf(RenderGraphBuilder& builder);
};

} // namespace Mizu