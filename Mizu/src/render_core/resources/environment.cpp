#include "environment.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/sampler_state.h"
#include "render_core/rhi/synchronization.h"

namespace Mizu
{

class IrradianceConvolutionShader : public ShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER("/EngineShaders/environment/IrradianceConvolution.vert.spv",
                              "vsMain",
                              "/EngineShaders/environment/IrradianceConvolution.frag.spv",
                              "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_IMAGE_VIEW(environmentMap)
        SHADER_PARAMETER_SAMPLER_STATE(sampler)
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

std::shared_ptr<Environment> Environment::create(const Cubemap::Faces& faces)
{
    const auto cubemap = Cubemap::create(faces, Renderer::get_allocator());
    return create_internal(cubemap);
}

std::shared_ptr<Environment> Environment::create(std::shared_ptr<Cubemap> cubemap)
{
    return create_internal(cubemap);
}

Environment::Environment(std::shared_ptr<Cubemap> cubemap, std::shared_ptr<Cubemap> irradiance_map)
    : m_cubemap(std::move(cubemap))
    , m_irradiance_map(std::move(irradiance_map))
{
}

std::shared_ptr<Environment> Environment::create_internal(std::shared_ptr<Cubemap> cubemap)
{
    RenderGraphBuilder builder;

    builder.start_debug_label("EnvironmentCreation");

    const RGCubemapRef& cubemap_ref = builder.register_external_cubemap(*cubemap);
    const RGImageViewRef& cubemap_view_ref =
        builder.create_image_view(cubemap_ref, ImageResourceViewRange::from_layers(0, 6));

    const auto irradiance_map = create_irradiance_map(builder, cubemap_view_ref);

    builder.end_debug_label();

    {
        const auto allocator = RenderGraphDeviceMemoryAllocator::create();
        const auto command_buffer = RenderCommandBuffer::create();

        auto fence = Mizu::Fence::create();
        fence->wait_for();

        const std::optional<RenderGraph>& graph = builder.compile(*allocator);
        MIZU_ASSERT(graph.has_value(), "Could not create Environment creation RenderGraph");

        CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = fence;
        graph->execute(*command_buffer, submit_info);

        fence->wait_for();
    }

    return std::shared_ptr<Environment>(new Environment(cubemap, irradiance_map));
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

    const auto irradiance_map = Cubemap::create(irradiance_map_desc, Renderer::get_allocator());

    const RGCubemapRef irradiance_map_ref = builder.register_external_cubemap(*irradiance_map);

    RGGraphicsPipelineDescription pipeline_desc{};

    IrradianceConvolutionShader::Parameters params{};
    params.environmentMap = cubemap_ref;
    params.sampler = RHIHelpers::get_sampler_state(SamplingOptions{});

    IrradianceConvolutionShader shader{};

    // clang-format off
    const std::vector<glm::vec3> vertices = {
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

    std::vector<uint32_t> indices = {
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

    const auto cube_vertex_buffer = VertexBuffer::create(vertices, Renderer::get_allocator());
    const auto cube_index_buffer = IndexBuffer::create(indices, Renderer::get_allocator());

    for (uint32_t i = 0; i < 6; ++i)
    {
        const RGImageViewRef view =
            builder.create_image_view(irradiance_map_ref, ImageResourceViewRange::from_layers(i, 1));
        const RGFramebufferRef framebuffer_ref =
            builder.create_framebuffer({IRRADIENCE_MAP_DIMENSIONS, IRRADIENCE_MAP_DIMENSIONS}, {view});

        const std::string pass_name = std::format("IrradianceConvolution_{}", i);
        builder.add_pass(pass_name, shader, params, pipeline_desc, framebuffer_ref, [=](RenderCommandBuffer& command) {
            struct IrradianceConvolutionInfo
            {
                glm::mat4 projection;
                glm::mat4 view;
            };

            IrradianceConvolutionInfo info{};
            info.projection = s_capture_projection;
            info.view = s_capture_views[i];

            command.push_constant("info", info);
            command.draw_indexed(*cube_vertex_buffer, *cube_index_buffer);
        });
    }

    return irradiance_map;
}

} // namespace Mizu