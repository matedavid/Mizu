#include "forward_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/system/forward_renderer_shaders.h"

#include "renderer/camera.h"
#include "renderer/render_graph/render_graph_builder.h"

#include "renderer/abstraction/buffers.h"
#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/synchronization.h"
#include "renderer/abstraction/texture.h"

#include "scene/scene.h"
#include "utility/assert.h"

namespace Mizu {

#define BIND_FUNC(func) \
    [&](auto x) {       \
        func(x);        \
    }

ForwardRenderer::ForwardRenderer(std::shared_ptr<Scene> scene, uint32_t width, uint32_t height)
      : m_scene(std::move(scene)) {
    m_camera_info_buffer = UniformBuffer::create<CameraInfoUBO>();
    m_fence = Fence::create();

    init(width, height);
}

ForwardRenderer::~ForwardRenderer() {
    // Wait for execution to end
    m_fence->wait_for();
}

void ForwardRenderer::render(const Camera& camera) {
    m_fence->wait_for();

    CameraInfoUBO camera_info{};
    camera_info.view = camera.view_matrix();
    camera_info.projection = camera.projection_matrix();

    m_camera_info_buffer->update(camera_info);

    CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = m_fence;

    m_graph.execute(submit_info);
}

void ForwardRenderer::init(uint32_t width, uint32_t height) {
    RenderGraphBuilder builder;

    ImageDescription output_description{};
    output_description.width = width;
    output_description.height = height;
    output_description.format = ImageFormat::BGRA8_SRGB;
    output_description.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled;

    m_output_texture = Texture2D::create(output_description);
    const RGUniformBufferRef camera_info_ref = builder.register_uniform_buffer(m_camera_info_buffer);

    const RGTextureRef output_texture_ref = builder.register_texture(m_output_texture);
    const RGTextureRef output_depth_texture_ref = builder.create_texture(width, height, ImageFormat::D32_SFLOAT);

    const RGFramebufferRef output_framebuffer =
        builder.create_framebuffer(width, height, {output_texture_ref, output_depth_texture_ref});

    {
        RGGraphicsPipelineDescription pipeline_description{};
        pipeline_description.depth_stencil = DepthStencilState{
            .depth_test = true,
            .depth_write = true,
        };

        ForwardRenderer_BasicShader::Parameters params{};
        params.uCameraInfo = camera_info_ref;

        builder.add_pass<ForwardRenderer_BasicShader>(
            "ForwardRenderer_OutputPass", pipeline_description, params, output_framebuffer, BIND_FUNC(render_models));
    }

    auto graph = RenderGraph::build(builder);
    MIZU_VERIFY(graph.has_value(), "Failed to build ForwardRenderer RenderGraph");
    m_graph = *graph;
}

void ForwardRenderer::render_models(std::shared_ptr<RenderCommandBuffer> command_buffer) const {
    struct ModelInfoData {
        glm::mat4 model;
    };

    for (const auto& entity : m_scene->view<MeshRendererComponent>()) {
        const MeshRendererComponent& mesh_renderer = entity.get_component<MeshRendererComponent>();
        const TransformComponent& transform = entity.get_component<TransformComponent>();

        glm::mat4 model(1.0f);
        model = glm::translate(model, transform.position);
        model = glm::rotate(model, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, transform.scale);

        ModelInfoData model_info{};
        model_info.model = model;
        command_buffer->push_constant("uModelInfo", model_info);

        command_buffer->draw_indexed(mesh_renderer.mesh->vertex_buffer(), mesh_renderer.mesh->index_buffer());
    }
}

} // namespace Mizu
