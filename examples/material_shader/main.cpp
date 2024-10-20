#include <Mizu/Mizu.h>
#include <Mizu/Plugins/AssimpLoader.h>
#include <Mizu/Plugins/CameraControllers.h>

#include <glm/gtc/matrix_transform.hpp>

class BaseShader : public Mizu::ShaderDeclaration<> {
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(uCameraInfo)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class PBRMaterialShader : public Mizu::MaterialShader<BaseShader> {
  public:
    IMPLEMENT_GRAPHICS_SHADER("/ExampleShadersPath/PBRShader.vert.spv",
                              "main",
                              "/ExampleShadersPath/PBRShader.frag.spv",
                              "main")

    // clang-format off
    BEGIN_MATERIAL_PARAMETERS()
        MATERIAL_PARAMETER_TEXTURE2D(uAlbedo)
    END_MATERIAL_PARAMETERS()
    // clang-format on
};

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

static std::filesystem::path s_example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera_controller =
            std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .lateral_rotation_sensitivity = 2.0f,
            .vertical_rotation_sensitivity = 2.0f,
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        const auto mesh_path = s_example_path / "cube.fbx";

        auto loader = Mizu::AssimpLoader::load(mesh_path);
        assert(loader.has_value());

        m_cube_mesh = loader->get_meshes()[0];

        Mizu::ShaderManager::create_shader_mapping("/ExampleShadersPath", s_example_path / "shaders");

        m_camera_ubo = Mizu::UniformBuffer::create<CameraUBO>();
        m_render_finished_semaphore = Mizu::Semaphore::create();
        m_render_finished_fence = Mizu::Fence::create();

        init(WIDTH, HEIGHT);
        m_presenter = Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_present_texture);
    }

    ~ExampleLayer() { 
        (void)5;
    }

    void on_update(double ts) override {
        m_camera_controller->update(ts);
        m_camera_ubo->update(CameraUBO{
            .view = m_camera_controller->view_matrix(),
            .projection = m_camera_controller->projection_matrix(),
        });

        m_render_finished_fence->wait_for();

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_semaphore = m_render_finished_semaphore;
        submit_info.signal_fence = m_render_finished_fence;

        m_graph.execute(submit_info);

        m_presenter->present(m_render_finished_semaphore);
    }

    void on_window_resized(Mizu::WindowResizeEvent& event) override {
        Mizu::Renderer::wait_idle();
        init(event.get_width(), event.get_height());
        m_presenter->texture_changed(m_present_texture);
        m_camera_controller->set_aspect_ratio(static_cast<float>(event.get_width())
                                              / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;
    std::shared_ptr<Mizu::Presenter> m_presenter;

    std::shared_ptr<Mizu::Texture2D> m_present_texture;
    std::shared_ptr<Mizu::UniformBuffer> m_camera_ubo;
    std::shared_ptr<Mizu::Semaphore> m_render_finished_semaphore;
    std::shared_ptr<Mizu::Fence> m_render_finished_fence;
    std::shared_ptr<Mizu::Mesh> m_cube_mesh;

    std::shared_ptr<Mizu::Material<PBRMaterialShader>> m_mat_1, m_mat_2;

    Mizu::RenderGraph m_graph;

    void init(uint32_t width, uint32_t height) {
        Mizu::RenderGraphBuilder builder;

        Mizu::ImageDescription texture_desc{};
        texture_desc.width = width;
        texture_desc.height = height;
        texture_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        texture_desc.usage = Mizu::ImageUsageBits::Attachment | Mizu::ImageUsageBits::Sampled;

        m_present_texture = Mizu::Texture2D::create(texture_desc);

        const Mizu::RGTextureRef present_texture_ref = builder.register_texture(m_present_texture);
        const Mizu::RGTextureRef depth_texture_ref =
            builder.create_texture(width, height, Mizu::ImageFormat::D32_SFLOAT);

        const Mizu::RGFramebufferRef present_framebuffer_ref =
            builder.create_framebuffer(width, height, {present_texture_ref, depth_texture_ref});

        const Mizu::RGUniformBufferRef camera_ubo_ref = builder.register_uniform_buffer(m_camera_ubo);

        PBRMaterialShader::Parameters texture_pass_params;
        texture_pass_params.uCameraInfo = camera_ubo_ref;

        Mizu::RGGraphicsPipelineDescription pipeline_desc{};
        pipeline_desc.depth_stencil.depth_test = false;
        pipeline_desc.depth_stencil.depth_write = false;

        PBRMaterialShader::MaterialParameters mat_params_1;
        mat_params_1.uAlbedo = Mizu::Texture2D::create(s_example_path / "texture_1.jpg", Mizu::SamplingOptions{});

        PBRMaterialShader::MaterialParameters mat_params_2;
        mat_params_2.uAlbedo = Mizu::Texture2D::create(s_example_path / "texture_2.jpg", Mizu::SamplingOptions{});

        m_mat_1 = std::make_shared<Mizu::Material<PBRMaterialShader>>();
        m_mat_1->init(mat_params_1);

        m_mat_2 = std::make_shared<Mizu::Material<PBRMaterialShader>>();
        m_mat_2->init(mat_params_2);

        builder.add_pass<PBRMaterialShader>(
            "PBR_MaterialPass",
            pipeline_desc,
            texture_pass_params,
            present_framebuffer_ref,
            [&](std::shared_ptr<Mizu::RenderCommandBuffer> command_buffer, Mizu::ApplyMaterialFunc apply_mat) {
                struct ModelInfoData {
                    glm::mat4 model;
                };

                {
                    glm::mat4 model_1(1.0f);
                    model_1 = glm::translate(model_1, glm::vec3(-2.0f, 0.0f, 0.0f));

                    command_buffer->push_constant("uModelInfo", ModelInfoData{.model = model_1});

                    apply_mat(command_buffer, *m_mat_1);
                    command_buffer->draw_indexed(m_cube_mesh->vertex_buffer(), m_cube_mesh->index_buffer());
                }

                {
                    glm::mat4 model_2(1.0f);
                    model_2 = glm::translate(model_2, glm::vec3(2.0f, 0.0f, 0.0f));

                    command_buffer->push_constant("uModelInfo", ModelInfoData{.model = model_2});

                    apply_mat(command_buffer, *m_mat_2);
                    command_buffer->draw_indexed(m_cube_mesh->vertex_buffer(), m_cube_mesh->index_buffer());
                }
            });

        auto graph = Mizu::RenderGraph::build(builder);
        assert(graph.has_value());

        m_graph = *graph;
    }
};

int main() {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Material Shaders";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
