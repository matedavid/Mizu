#include "deferred_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/deferred/deferred_renderer_shaders.h"

#include "render_core/resources/camera.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/synchronization.h"

#include "scene/scene.h"

namespace Mizu
{

struct CameraUBO
{
    glm::mat4 view;
    glm::mat4 projection;
};

struct ModelInfoData
{
    glm::mat4 model;
};

DeferredRenderer::DeferredRenderer(std::shared_ptr<Scene> scene, uint32_t width, uint32_t height)
    : m_scene(std::move(scene))
    , m_dimensions({width, height})
{
    m_command_buffer = RenderCommandBuffer::create();
    m_rg_allocator = RenderGraphDeviceMemoryAllocator::create();

    m_fence = Fence::create();
    m_render_semaphore = Semaphore::create();

    m_camera_ubo = UniformBuffer::create<CameraUBO>(Renderer::get_allocator());

    Texture2D::Description desc{};
    desc.dimensions = m_dimensions;
    desc.format = ImageFormat::RGBA8_SRGB;
    desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled;

    m_result_texture = Texture2D::create(desc, SamplingOptions{}, Renderer::get_allocator());
}

DeferredRenderer::~DeferredRenderer() {}

void DeferredRenderer::render(const Camera& camera)
{
    m_fence->wait_for();

    CameraUBO camera_info{};
    camera_info.view = camera.view_matrix();
    camera_info.projection = camera.projection_matrix();

    m_camera_ubo->update(camera_info);

    //
    // Create RenderGraph
    //

    RenderGraphBuilder builder;

    const RGBufferRef camera_ubo_ref = builder.register_external_buffer(*m_camera_ubo);
    const RGBufferRef result_texture_ref = builder.register_external_texture(*m_result_texture);

    const RGTextureRef& depth_prepass_ref = add_depth_prepass(builder, camera_ubo_ref);
    add_simple_color_pass(builder, camera_ubo_ref, depth_prepass_ref, result_texture_ref);

    //
    // Compile & Execute
    //

    const std::optional<RenderGraph> graph = builder.compile(m_command_buffer, *m_rg_allocator);
    MIZU_ASSERT(graph.has_value(), "Could not compile RenderGraph");

    m_graph = *graph;

    CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = m_fence;
    submit_info.signal_semaphore = m_render_semaphore;

    m_graph.execute(submit_info);
}

void DeferredRenderer::resize(uint32_t width, uint32_t height)
{
    m_dimensions = glm::uvec2(width, height);

    Texture2D::Description desc{};
    desc.dimensions = m_dimensions;
    desc.format = ImageFormat::RGBA8_SRGB;
    desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled;

    m_result_texture = Texture2D::create(desc, SamplingOptions{}, Renderer::get_allocator());
}

RGTextureRef DeferredRenderer::add_depth_prepass(RenderGraphBuilder& builder, const RGBufferRef& camera_ubo_ref) const
{
    const RGTextureRef depth_prepass_ref =
        builder.create_texture<Texture2D>(m_dimensions, ImageFormat::D32_SFLOAT, SamplingOptions{});

    RGGraphicsPipelineDescription pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = true,
    };

    const RGFramebufferRef framebuffer_ref = builder.create_framebuffer(m_dimensions, {depth_prepass_ref});

    Deferred_DepthPrePass::Parameters params{};
    params.uCameraInfo = camera_ubo_ref;

    builder.add_pass<Deferred_DepthPrePass>(
        "DepthPrePass", params, pipeline, framebuffer_ref, [&](RenderCommandBuffer& command) {
            for (const Entity& entity : m_scene->view<MeshRendererComponent>())
            {
                const Mizu::MeshRendererComponent& mesh_renderer = entity.get_component<Mizu::MeshRendererComponent>();
                const Mizu::TransformComponent& transform = entity.get_component<Mizu::TransformComponent>();

                glm::mat4 model(1.0f);
                model = glm::translate(model, transform.position);
                model = glm::rotate(model, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, transform.scale);

                ModelInfoData model_info{};
                model_info.model = model;
                command.push_constant("uModelInfo", model_info);

                command.draw_indexed(mesh_renderer.mesh->vertex_buffer(), mesh_renderer.mesh->index_buffer());
            }
        });

    return depth_prepass_ref;
}

void DeferredRenderer::add_simple_color_pass(RenderGraphBuilder& builder,
                                             const RGBufferRef& camera_ubo_ref,
                                             const RGTextureRef& depth_prepass_ref,
                                             const RGTextureRef& result_texture_ref) const
{
    RGGraphicsPipelineDescription pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = false,
        .depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual,
    };
    const RGFramebufferRef framebuffer_ref =
        builder.create_framebuffer(m_dimensions, {result_texture_ref, depth_prepass_ref});

    Deferred_SimpleColor::Parameters params{};
    params.uCameraInfo = camera_ubo_ref;

    builder.add_pass<Deferred_SimpleColor>(
        "SimpleColor", params, pipeline, framebuffer_ref, [&](RenderCommandBuffer& command) {
            for (const Entity& entity : m_scene->view<MeshRendererComponent>())
            {
                const Mizu::MeshRendererComponent& mesh_renderer = entity.get_component<Mizu::MeshRendererComponent>();
                const Mizu::TransformComponent& transform = entity.get_component<Mizu::TransformComponent>();

                glm::mat4 model(1.0f);
                model = glm::translate(model, transform.position);
                model = glm::rotate(model, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, transform.scale);

                ModelInfoData model_info{};
                model_info.model = model;
                command.push_constant("uModelInfo", model_info);

                command.draw_indexed(mesh_renderer.mesh->vertex_buffer(), mesh_renderer.mesh->index_buffer());
            }
        });
}

} // namespace Mizu