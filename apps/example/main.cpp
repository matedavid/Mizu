#include "Mizu/Mizu.h"

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

struct ExampleVertex {
    glm::vec3 pos;
};

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {
        m_command_buffer = Mizu::RenderCommandBuffer::create();

        m_texture = Mizu::Texture2D::create(Mizu::ImageDescription{
            .width = WIDTH,
            .height = HEIGHT,
            .usage = Mizu::ImageUsageBits::Sampled | Mizu::ImageUsageBits::Attachment,
        });
        assert(m_texture != nullptr);

        Mizu::Framebuffer::Attachment texture_attachment{};
        texture_attachment.image = m_texture;
        texture_attachment.clear_value = glm::vec3(0.0f);
        texture_attachment.load_operation = Mizu::LoadOperation::Clear;
        texture_attachment.store_operation = Mizu::StoreOperation::Store;

        m_framebuffer = Mizu::Framebuffer::create(Mizu::Framebuffer::Description{
            .attachments = {texture_attachment},
            .width = WIDTH,
            .height = HEIGHT,
        });
        assert(m_framebuffer != nullptr);

        m_render_pass = Mizu::RenderPass::create(Mizu::RenderPass::Description{
            .debug_name = "ExampleRenderPass",
            .target_framebuffer = m_framebuffer,
        });

        m_graphics_pipeline = Mizu::GraphicsPipeline::create(Mizu::GraphicsPipeline::Description{
            .shader = Mizu::Shader::create("../../apps/example/simple.vert.spv", "../../apps/example/simple.frag.spv"),
            .target_framebuffer = m_framebuffer,
        });

        const std::vector<ExampleVertex> vertex_data = {
            ExampleVertex{.pos = glm::vec3(-0.5f, 0.5f, 0.0f)},
            ExampleVertex{.pos = glm::vec3(0.5f, 0.5f, 0.0f)},
            ExampleVertex{.pos = glm::vec3(0.0f, -0.5f, 0.0f)},
        };
        const std::vector<Mizu::VertexBuffer::Layout> vertex_layout = {
            {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
        };
        m_vertex_buffer = Mizu::VertexBuffer::create(vertex_data, vertex_layout);

        m_fence = Mizu::Fence::create();

        m_presenter = Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_texture);
    }

    void on_update(double ts) override {
        m_fence->wait_for();

        m_command_buffer->begin();
        {
            m_command_buffer->begin_render_pass(m_render_pass);

            m_command_buffer->bind_pipeline(m_graphics_pipeline);
            m_command_buffer->draw(m_vertex_buffer);

            m_command_buffer->end_render_pass(m_render_pass);
        }
        m_command_buffer->end();

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fence;

        m_command_buffer->submit(submit_info);

        m_presenter->present();
    }

  private:
    std::shared_ptr<Mizu::RenderCommandBuffer> m_command_buffer;

    std::shared_ptr<Mizu::RenderPass> m_render_pass;
    std::shared_ptr<Mizu::GraphicsPipeline> m_graphics_pipeline;
    std::shared_ptr<Mizu::Texture2D> m_texture;
    std::shared_ptr<Mizu::Framebuffer> m_framebuffer;
    std::shared_ptr<Mizu::VertexBuffer> m_vertex_buffer;

    std::shared_ptr<Mizu::Presenter> m_presenter;
    std::shared_ptr<Mizu::Fence> m_fence;
};

int main() {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Example";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
