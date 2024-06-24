#include "Mizu/Mizu.h"

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

struct ExampleVertex {
    glm::vec3 pos;
};

struct ExampleCameraUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

class ExampleLayer : public Mizu::Layer {
  public:
    Mizu::RenderGraph graph;

    ExampleLayer() {
        m_command_buffer = Mizu::RenderCommandBuffer::create();

        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera = std::make_shared<Mizu::PerspectiveCamera>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera->set_position({0.0f, 0.0f, 1.0f});

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

        m_camera_info_ubo = Mizu::UniformBuffer::create<ExampleCameraUBO>();
        m_camera_resource_group = Mizu::ResourceGroup::create();
        m_camera_resource_group->add_resource("uCameraInfo", m_camera_info_ubo);

        init(WIDTH, HEIGHT);
        m_presenter = Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_texture);
    }

    void on_update(double ts) override {
        m_fence->wait_for();

        ExampleCameraUBO camera_info{};
        camera_info.view = m_camera->view_matrix();
        camera_info.projection = m_camera->projection_matrix();

        m_camera_info_ubo->update(camera_info);

        /*
        m_command_buffer->begin();
        {
            m_command_buffer->begin_render_pass(m_render_pass);

            m_command_buffer->bind_pipeline(m_graphics_pipeline);

            struct ModelInfo {
                glm::mat4 model;
            };

            ModelInfo model_info{};
            model_info.model = glm::mat4(1.0f);

            m_command_buffer->bind_resource_group(m_camera_resource_group, 0);
            m_graphics_pipeline->push_constant(m_command_buffer, "uModelInfo", model_info);

            m_command_buffer->draw(m_vertex_buffer);

            m_command_buffer->end_render_pass(m_render_pass);
        }
        m_command_buffer->end();
        */

        /*
        m_command_buffer->submit(submit_info);
        */

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fence;

        graph.execute(submit_info);

        m_presenter->present();
    }

    void on_window_resized(Mizu::WindowResizeEvent& event) override {
        Mizu::Renderer::wait_idle();
        init(event.get_width(), event.get_height());
        m_presenter->texture_changed(m_texture);
        m_camera->set_aspect_ratio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
    }

    void on_key_pressed(Mizu::KeyPressedEvent& event) override {
        glm::vec3 pos = m_camera->get_position();

        if (event.get_key() == Mizu::Key::S) {
            pos.z += 1.0f;
        }

        if (event.get_key() == Mizu::Key::W) {
            pos.z -= 1.0f;
        }

        if (event.get_key() == Mizu::Key::A) {
            pos.x -= 1.0f;
        }

        if (event.get_key() == Mizu::Key::D) {
            pos.x += 1.0f;
        }

        m_camera->set_position(pos);
    }

  private:
    std::shared_ptr<Mizu::RenderCommandBuffer> m_command_buffer;
    std::shared_ptr<Mizu::Fence> m_fence;

    std::shared_ptr<Mizu::PerspectiveCamera> m_camera;

    std::shared_ptr<Mizu::RenderPass> m_render_pass;
    std::shared_ptr<Mizu::GraphicsPipeline> m_graphics_pipeline;
    std::shared_ptr<Mizu::Texture2D> m_texture;
    std::shared_ptr<Mizu::Framebuffer> m_framebuffer;
    std::shared_ptr<Mizu::VertexBuffer> m_vertex_buffer;

    std::shared_ptr<Mizu::UniformBuffer> m_camera_info_ubo;
    std::shared_ptr<Mizu::ResourceGroup> m_camera_resource_group;

    std::shared_ptr<Mizu::Presenter> m_presenter;

    void init(uint32_t width, uint32_t height) {
        m_texture = Mizu::Texture2D::create(Mizu::ImageDescription{
            .width = width,
            .height = height,
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
            .width = width,
            .height = height,
        });
        assert(m_framebuffer != nullptr);

        const auto pipeline_desc = Mizu::GraphicsPipeline::Description{
            .shader = Mizu::Shader::create("../../apps/example/simple.vert.spv", "../../apps/example/simple.frag.spv"),
            .target_framebuffer = m_framebuffer,
            .depth_stencil =
                Mizu::DepthStencilState{
                    .depth_test = false,
                },
        };

        graph.add_pass(
            "ExampleRenderPass", pipeline_desc, m_framebuffer, [&](std::shared_ptr<Mizu::RenderCommandBuffer> cb) {
                struct ModelInfo {
                    glm::mat4 model;
                };

                ModelInfo model_info{};
                model_info.model = glm::mat4(1.0f);

                cb->bind_resource_group(m_camera_resource_group, 0);
                cb->push_constant("uModelInfo", model_info);

                cb->draw(m_vertex_buffer);
            });

        /*
        m_render_pass = Mizu::RenderPass::create(Mizu::RenderPass::Description{
            .debug_name = "ExampleRenderPass",
            .target_framebuffer = m_framebuffer,
        });

        m_graphics_pipeline = Mizu::GraphicsPipeline::create(Mizu::GraphicsPipeline::Description{
            .shader = Mizu::Shader::create("../../apps/example/simple.vert.spv", "../../apps/example/simple.frag.spv"),
            .target_framebuffer = m_framebuffer,
            .depth_stencil = Mizu::DepthStencilState{
                .depth_test = false,
            }});
        */
    }
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
