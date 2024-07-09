#include "Mizu/Mizu.h"

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class ExampleBaseShader : public Mizu::ShaderDeclaration<> {
  public:
    BEGIN_SHADER_PARAMETERS()
    END_SHADER_PARAMETERS()
};

class ColorShader : public Mizu::ShaderDeclaration<ExampleBaseShader> {
  public:
    IMPLEMENT_SHADER("ExampleShaders/simple.vert.spv", "ExampleShaders/simple.frag.spv");

    BEGIN_SHADER_PARAMETERS()
    END_SHADER_PARAMETERS()
};

class InvertShader : public Mizu::ShaderDeclaration<ExampleBaseShader> {
  public:
    IMPLEMENT_SHADER("ExampleShaders/invert.vert.spv", "ExampleShaders/invert.frag.spv");

    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_TEXTURE2D(uInput)
    END_SHADER_PARAMETERS()
    // clang-format on
};

struct ExampleVertex {
    glm::vec3 pos;
    glm::vec2 tex;
};

struct ExampleCameraUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

class ExampleLayer : public Mizu::Layer {
  public:
    Mizu::RenderGraph m_graph;

    ExampleLayer() {
        Mizu::ShaderManager::create_shader_mapping("ExampleShaders", "../../apps/example/");

        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera = std::make_shared<Mizu::PerspectiveCamera>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera->set_position({0.0f, 0.0f, 1.0f});

        const std::vector<Mizu::VertexBuffer::Layout> vertex_layout = {
            {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
            {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
        };

        const std::vector<ExampleVertex> triangle_vertex_data = {
            ExampleVertex{.pos = glm::vec3(-0.5f, 0.5f, 0.0f), .tex = glm::vec2(0.0f)},
            ExampleVertex{.pos = glm::vec3(0.5f, 0.5f, 0.0f), .tex = glm::vec2(0.0f)},
            ExampleVertex{.pos = glm::vec3(0.0f, -0.5f, 0.0f), .tex = glm::vec2(0.0f)},
        };
        m_triangle_vertex_buffer = Mizu::VertexBuffer::create(triangle_vertex_data, vertex_layout);

        const std::vector<ExampleVertex> fullscreen_quad_vertex_data = {
            ExampleVertex{.pos = glm::vec3(0.0f, 1.0f, 0.0f), .tex = glm::vec2(0.5f, 1.0f)},
            ExampleVertex{.pos = glm::vec3(1.0f, 1.0f, 0.0f), .tex = glm::vec2(1.0f, 1.0f)},
            ExampleVertex{.pos = glm::vec3(1.0f, 0.0f, 0.0f), .tex = glm::vec2(1.0f, 0.0f)},

            ExampleVertex{.pos = glm::vec3(1.0f, 0.0f, 0.0f), .tex = glm::vec2(1.0f, 0.0f)},
            ExampleVertex{.pos = glm::vec3(-1.0f, 0.0f, 0.0f), .tex = glm::vec2(0.0f, 0.0f)},
            ExampleVertex{.pos = glm::vec3(-1.0f, 1.0f, 0.0f), .tex = glm::vec2(0.0f, 1.0f)},
        };
        m_fullscreen_vertex_buffer = Mizu::VertexBuffer::create(fullscreen_quad_vertex_data, vertex_layout);

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

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fence;

        m_graph.execute(submit_info);

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
    std::shared_ptr<Mizu::Fence> m_fence;

    std::shared_ptr<Mizu::PerspectiveCamera> m_camera;

    std::shared_ptr<Mizu::Texture2D> m_texture;

    std::shared_ptr<Mizu::VertexBuffer> m_triangle_vertex_buffer;
    std::shared_ptr<Mizu::VertexBuffer> m_fullscreen_vertex_buffer;

    std::shared_ptr<Mizu::UniformBuffer> m_camera_info_ubo;
    std::shared_ptr<Mizu::ResourceGroup> m_camera_resource_group;

    std::shared_ptr<Mizu::Presenter> m_presenter;

    void init(uint32_t width, uint32_t height) {
        Mizu::RenderGraphBuilder builder;

        m_texture = Mizu::Texture2D::create(Mizu::ImageDescription{
            .width = width,
            .height = height,
            .usage = Mizu::ImageUsageBits::Sampled | Mizu::ImageUsageBits::Attachment,
        });
        assert(m_texture != nullptr);

        auto color_texture_id = builder.create_texture(width, height, Mizu::ImageFormat::RGBA8_SRGB);
        auto invert_color_id = builder.register_texture(m_texture);

        auto color_framebuffer_id = builder.create_framebuffer(width, height, {color_texture_id});
        auto invert_framebuffer_id = builder.create_framebuffer(width, height, {invert_color_id});

        // Color pass
        {
            const auto render_triangle_func = [&](std::shared_ptr<Mizu::RenderCommandBuffer> cb) {
                struct ModelInfo {
                    glm::mat4 model;
                };

                ModelInfo model_info{};
                model_info.model = glm::mat4(1.0f);

                cb->push_constant("uModelInfo", model_info);

                cb->draw(m_triangle_vertex_buffer);
            };

            const auto pipeline_desc = Mizu::RGGraphicsPipelineDescription{
                .depth_stencil = Mizu::DepthStencilState{.depth_test = true, .depth_write = false},
            };

            auto params = ColorShader::Parameters{};

            builder.add_pass<ColorShader>(
                "ExampleColorPass", pipeline_desc, params, color_framebuffer_id, render_triangle_func);
        }

        // Invert pass
        {
            const auto fullscreen_quad_func = [&](std::shared_ptr<Mizu::RenderCommandBuffer> cb) {
                cb->draw(m_fullscreen_vertex_buffer);
            };

            const auto pipeline_desc = Mizu::RGGraphicsPipelineDescription{
                .depth_stencil = Mizu::DepthStencilState{.depth_test = false, .depth_write = false},
            };

            auto params = InvertShader::Parameters{};
            params.uInput = color_texture_id;

            builder.add_pass<InvertShader>(
                "ExampleInvertPass", pipeline_desc, params, invert_framebuffer_id, fullscreen_quad_func);
        }

        auto graph = Mizu::RenderGraph::build(builder);
        assert(graph.has_value());
        m_graph = *graph;
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
