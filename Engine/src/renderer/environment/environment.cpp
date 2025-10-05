#include "environment.h"

#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_utils.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/sampler_state.h"
#include "render_core/rhi/synchronization.h"

namespace Mizu
{

class IrradianceConvolutionShader : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION(
        "/EngineShaders/environment/IrradianceConvolution.vert.spv",
        "vsMain",
        "/EngineShaders/environment/IrradianceConvolution.frag.spv",
        "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(environmentMap)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)

        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class PrefilterEnvironmentShader : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION(
        "/EngineShaders/environment/PrefilterEnvironment.vert.spv",
        "vsMain",
        "/EngineShaders/environment/PrefilterEnvironment.frag.spv",
        "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(environmentMap)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)

        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class PrecomputeBRDFShader : public ComputeShaderDeclaration
{
  public:
    IMPLEMENT_COMPUTE_SHADER_DECLARATION("/EngineShaders/environment/PrecomputeBRDF.comp.spv", "precomputeBRDF")

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(output)
    END_SHADER_PARAMETERS()
    // clang-format on
};

static glm::mat4 s_capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
static glm::mat4 s_capture_views[] = {
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
};

static std::shared_ptr<VertexBuffer> s_cube_vertex_buffer = nullptr;
static std::shared_ptr<IndexBuffer> s_cube_index_buffer = nullptr;

std::shared_ptr<Environment> Environment::create(const Cubemap::Faces& faces)
{
    const auto cubemap = Cubemap::create(faces);
    return create_internal(cubemap);
}

std::shared_ptr<Environment> Environment::create(std::shared_ptr<Cubemap> cubemap)
{
    return create_internal(cubemap);
}

Environment::Environment(
    std::shared_ptr<Cubemap> cubemap,
    std::shared_ptr<Cubemap> irradiance_map,
    std::shared_ptr<Cubemap> prefiltered_environment_map,
    std::shared_ptr<Texture2D> precomputed_brdf)
    : m_cubemap(std::move(cubemap))
    , m_irradiance_map(std::move(irradiance_map))
    , m_prefiltered_environment_map(std::move(prefiltered_environment_map))
    , m_precomputed_brdf(std::move(precomputed_brdf))

{
}

std::shared_ptr<Environment> Environment::create_internal(std::shared_ptr<Cubemap> cubemap)
{
    // clang-format off
    const std::vector<glm::vec3> cube_vertices = {
        // Front face
        {-0.5f, -0.5f,  0.5f}, // 0
        { 0.5f, -0.5f,  0.5f}, // 1
        { 0.5f,  0.5f,  0.5f}, // 2
        {-0.5f,  0.5f,  0.5f}, // 3

        // Back face
        {-0.5f, -0.5f, -0.5f}, // 4
        { 0.5f, -0.5f, -0.5f}, // 5
        { 0.5f,  0.5f, -0.5f}, // 6
        {-0.5f,  0.5f, -0.5f}  // 7
    };

    const std::vector<uint32_t> cube_indices = {
        // Front face
        0, 1, 2,   0, 2, 3,
        // Right face
        1, 5, 6,   1, 6, 2,
        // Back face
        5, 4, 7,   5, 7, 6,
        // Left face
        4, 0, 3,   4, 3, 7,
        // Top face
        3, 2, 6,   3, 6, 7,
        // Bottom face
        4, 5, 1,   4, 1, 0
    };
    // clang-format on

    s_cube_vertex_buffer = VertexBuffer::create(cube_vertices);
    s_cube_index_buffer = IndexBuffer::create(cube_indices);

    RenderGraphBuilder builder;

    builder.begin_gpu_marker("EnvironmentCreation");

    const RGImageRef& cubemap_ref = builder.register_external_cubemap(*cubemap);
    const RGImageViewRef& cubemap_view_ref =
        builder.create_image_view(cubemap_ref, ImageResourceViewRange::from_layers(0, 6));

    const auto irradiance_map = create_irradiance_map(builder, cubemap_view_ref);
    const auto prefiltered_environment_map = create_prefiltered_environment_map(builder, cubemap_view_ref);
    const auto precomputed_brdf = create_precomputed_brdf(builder);

    builder.end_gpu_marker();

    {
        const auto allocator = AliasedDeviceMemoryAllocator::create();
        const auto staging_allocator = AliasedDeviceMemoryAllocator::create();

        const auto command_buffer = RenderCommandBuffer::create();

        auto fence = Fence::create();
        fence->wait_for();

        const RenderGraphBuilderMemory builder_memory = RenderGraphBuilderMemory{*allocator, *staging_allocator};

        RenderGraph graph;
        builder.compile(graph, builder_memory);

        CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = fence;
        graph.execute(*command_buffer, submit_info);

        fence->wait_for();
    }

    s_cube_vertex_buffer = nullptr;
    s_cube_index_buffer = nullptr;

    return std::shared_ptr<Environment>(
        new Environment(cubemap, irradiance_map, prefiltered_environment_map, precomputed_brdf));
}

std::shared_ptr<Cubemap> Environment::create_irradiance_map(RenderGraphBuilder& builder, RGImageViewRef cubemap_ref)
{
    constexpr uint32_t IRRADIENCE_MAP_DIMENSIONS = 32;

    const std::string name = std::format("{}_IrradianceMap", "Skybox");

    Cubemap::Description irradiance_map_desc{};
    irradiance_map_desc.dimensions = {IRRADIENCE_MAP_DIMENSIONS, IRRADIENCE_MAP_DIMENSIONS};
    irradiance_map_desc.format = ImageFormat::RGBA32_SFLOAT;
    irradiance_map_desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled;
    irradiance_map_desc.name = name;

    const auto irradiance_map = Cubemap::create(irradiance_map_desc);

    const RGImageRef irradiance_map_ref = builder.register_external_cubemap(*irradiance_map);

    GraphicsPipeline::Description pipeline_desc{};

    IrradianceConvolutionShader::Parameters params{};
    params.environmentMap = cubemap_ref;
    params.sampler = RHIHelpers::get_sampler_state(SamplingOptions{});
    params.framebuffer = RGFramebufferAttachments{
        .width = IRRADIENCE_MAP_DIMENSIONS,
        .height = IRRADIENCE_MAP_DIMENSIONS,
    };

    IrradianceConvolutionShader shader{};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const RGImageViewRef view =
            builder.create_image_view(irradiance_map_ref, ImageResourceViewRange::from_layers(i, 1));

        params.framebuffer.color_attachments = {view};

        const std::string pass_name = std::format("IrradianceConvolution_{}", i);
        add_raster_pass(
            builder,
            pass_name,
            shader,
            params,
            pipeline_desc,
            [=](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
                struct IrradianceConvolutionInfo
                {
                    glm::mat4 projection;
                    glm::mat4 view;
                };

                IrradianceConvolutionInfo info{};
                info.projection = s_capture_projection;
                info.view = s_capture_views[i];

                command.push_constant("info", info);
                command.draw_indexed(*s_cube_vertex_buffer, *s_cube_index_buffer);
            });
    }

    return irradiance_map;
}

std::shared_ptr<Cubemap> Environment::create_prefiltered_environment_map(
    RenderGraphBuilder& builder,
    RGImageViewRef cubemap_ref)
{
    constexpr uint32_t PREFILTERED_ENVIRONMENT_MAP_DIMENSIONS = 512;
    constexpr uint32_t MIP_LEVELS = 5;

    const std::string name = std::format("{}_PrefilteredEnvironmentMap", "Skybox");

    Cubemap::Description prefiltered_desc{};
    prefiltered_desc.dimensions = {PREFILTERED_ENVIRONMENT_MAP_DIMENSIONS, PREFILTERED_ENVIRONMENT_MAP_DIMENSIONS};
    prefiltered_desc.format = ImageFormat::RGBA32_SFLOAT;
    prefiltered_desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled;
    prefiltered_desc.num_mips = MIP_LEVELS;
    prefiltered_desc.name = name;

    const auto prefiltered_environment_map = Cubemap::create(prefiltered_desc);

    const RGImageRef prefiltered_environment_map_ref =
        builder.register_external_cubemap(*prefiltered_environment_map);

    GraphicsPipeline::Description pipeline_desc{};

    PrefilterEnvironmentShader::Parameters prefilter_environment_params{};
    prefilter_environment_params.environmentMap = cubemap_ref;
    prefilter_environment_params.sampler = RHIHelpers::get_sampler_state(SamplingOptions{});

    PrefilterEnvironmentShader prefilter_environment_shader{};

    for (uint32_t mip = 0; mip < MIP_LEVELS; ++mip)
    {
        const uint32_t mip_width = static_cast<uint32_t>(PREFILTERED_ENVIRONMENT_MAP_DIMENSIONS * std::pow(0.5f, mip));
        const uint32_t mip_height = static_cast<uint32_t>(PREFILTERED_ENVIRONMENT_MAP_DIMENSIONS * std::pow(0.5f, mip));

        for (uint32_t layer = 0; layer < 6; ++layer)
        {
            const RGImageViewRef view = builder.create_image_view(
                prefiltered_environment_map_ref, ImageResourceViewRange::from_mips_layers(mip, 1, layer, 1));

            prefilter_environment_params.framebuffer = RGFramebufferAttachments{
                .width = mip_width,
                .height = mip_height,
                .color_attachments = {view},
            };

            const float roughness = static_cast<float>(mip) / static_cast<float>(MIP_LEVELS - 1);

            const std::string pass_name = std::format("PrefilterEnvironment_{}_{}", mip, layer);
            add_raster_pass(
                builder,
                pass_name,
                prefilter_environment_shader,
                prefilter_environment_params,
                pipeline_desc,
                [=](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
                    struct PrefilterEnvironmentInfo
                    {
                        glm::mat4 view_projection;
                        float roughness;
                    };

                    PrefilterEnvironmentInfo info{};
                    info.view_projection = s_capture_projection * glm::mat4(glm::mat3(s_capture_views[layer]));
                    info.roughness = roughness;

                    command.push_constant("info", info);
                    command.draw_indexed(*s_cube_vertex_buffer, *s_cube_index_buffer);
                });
        }
    }

    return prefiltered_environment_map;
}

std::shared_ptr<Texture2D> Environment::create_precomputed_brdf(RenderGraphBuilder& builder)
{
    constexpr uint32_t PRECOMPUTED_BRDF_DIMENSIONS = 512;

    Texture2D::Description precomputed_brdf_desc{};
    precomputed_brdf_desc.dimensions = {PRECOMPUTED_BRDF_DIMENSIONS, PRECOMPUTED_BRDF_DIMENSIONS};
    precomputed_brdf_desc.format = ImageFormat::RG16_SFLOAT;
    precomputed_brdf_desc.usage = ImageUsageBits::Storage | ImageUsageBits::Sampled;
    precomputed_brdf_desc.name = "PrecomputedBRDF";

    const auto precomputed_brdf = Texture2D::create(precomputed_brdf_desc);

    const RGImageRef precomputed_brdf_ref =
        builder.register_external_texture(*precomputed_brdf, {.input_state = ImageResourceState::Undefined});

    PrecomputeBRDFShader::Parameters params{};
    params.output = builder.create_image_view(precomputed_brdf_ref);

    PrecomputeBRDFShader shader{};

    constexpr uint32_t GROUP_SIZE = 16;
    const glm::uvec3 group_count = RHIHelpers::compute_group_count(
        glm::uvec3(PRECOMPUTED_BRDF_DIMENSIONS, PRECOMPUTED_BRDF_DIMENSIONS, 1), {GROUP_SIZE, GROUP_SIZE, 1});
    MIZU_RG_ADD_COMPUTE_PASS(builder, "PrecomputeBRDF", shader, params, group_count);

    return precomputed_brdf;
}

} // namespace Mizu