#include <iostream>

#include <Mizu/Mizu.h>

class PruebasRenderer {
  public:
    PruebasRenderer() {
        m_command_buffer = Mizu::RenderCommandBuffer::create();

        Mizu::ImageDescription color_desc{};
        color_desc.width = 960;
        color_desc.height = 480;
        color_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        color_desc.attachment = true;

        m_color_texture = Mizu::Texture2D::create(color_desc);

        Mizu::Framebuffer::Attachment color_attachment{};
        color_attachment.image = m_color_texture;
        color_attachment.load_operation = Mizu::LoadOperation::DontCare;
        color_attachment.store_operation = Mizu::StoreOperation::Store;
        color_attachment.clear_value = glm::vec3(0.0f);

        Mizu::Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = 960;
        framebuffer_desc.height = 480;
        framebuffer_desc.attachments = {color_attachment};

        m_framebuffer = Mizu::Framebuffer::create(framebuffer_desc);

        const auto shader = Mizu::Shader::create("vertex.spv", "fragment.spv");

        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = m_framebuffer;
        pipeline_desc.depth_stencil = Mizu::DepthStencilState{
            .depth_test = false,
            .depth_write = false,
        };

        m_pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);

        Mizu::RenderPass::Description render_pass_desc{};
        render_pass_desc.debug_name = "PruebasRenderPass";
        render_pass_desc.target_framebuffer = m_framebuffer;

        m_render_pass = Mizu::RenderPass::create(render_pass_desc);

        const std::vector<Vertex> vertex_data = {
            Vertex{.position = glm::vec3{-0.5, 0.5f, 0.0}},
            Vertex{.position = glm::vec3{0.5, 0.5f, 0.0}},
            Vertex{.position = glm::vec3{0.0, -0.5f, 0.0}},
        };
        m_vertex_buffer = Mizu::VertexBuffer::create(vertex_data);
        m_index_buffer = Mizu::IndexBuffer::create(std::vector<uint32_t>{0, 1, 2});

        m_flight_fence = Mizu::Fence::create();
    }

    void render() {
        m_flight_fence->wait_for();

        m_command_buffer->begin();
        {
            m_render_pass->begin(m_command_buffer);

            m_pipeline->bind(m_command_buffer);
            Mizu::draw_indexed(m_command_buffer, m_vertex_buffer, m_index_buffer);

            m_render_pass->end(m_command_buffer);
        }
        m_command_buffer->end();

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_flight_fence;

        m_command_buffer->submit(submit_info);
    }

  private:
    std::shared_ptr<Mizu::Texture2D> m_color_texture;
    std::shared_ptr<Mizu::Framebuffer> m_framebuffer;

    std::shared_ptr<Mizu::RenderPass> m_render_pass;
    std::shared_ptr<Mizu::GraphicsPipeline> m_pipeline;

    std::shared_ptr<Mizu::RenderCommandBuffer> m_command_buffer;

    struct Vertex {
        glm::vec3 position;
    };
    std::shared_ptr<Mizu::VertexBuffer> m_vertex_buffer;
    std::shared_ptr<Mizu::IndexBuffer> m_index_buffer;

    std::shared_ptr<Mizu::Fence> m_flight_fence;
};

int main() {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{
        .graphics = true,
        .compute = false,
    };

    config.application_name = "Pruebas";
    config.application_version = Mizu::Version{0, 1, 0};

    bool ok = Mizu::initialize(config);
    if (!ok) {
        std::cout << "Failed to initialize Mizu\n";
        return 1;
    }

    {
        uint32_t i = 0;
        PruebasRenderer pruebas{};

        while (i < 10000000) {
            pruebas.render();
            i += 1;
        }
    }

    Mizu::shutdown();
    return 0;
}
