#include <Mizu/Mizu.h>
#include <Mizu/Plugins/AssimpLoader.h>
#include <Mizu/Plugins/CameraControllers.h>

#include <glm/gtc/matrix_transform.hpp>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class BaseShader : public Mizu::ShaderDeclaration<> {
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(uCameraInfo)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class NormalShader : public Mizu::ShaderDeclaration<BaseShader> {
  public:
    IMPLEMENT_GRAPHICS_SHADER("/ExampleShadersPath/NormalShader.vert.spv", "/ExampleShadersPath/NormalShader.frag.spv")

    // clang-format off
    BEGIN_SHADER_PARAMETERS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class SkyboxShader : public Mizu::ShaderDeclaration<BaseShader> {
  public:
    IMPLEMENT_GRAPHICS_SHADER("/ExampleShadersPath/Skybox.vert.spv", "/ExampleShadersPath/Skybox.frag.spv")

    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_CUBEMAP(uSkybox)
    END_SHADER_PARAMETERS()
    // clang-format on
};

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera_controller =
            std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);
        Mizu::ShaderManager::create_shader_mapping("/ExampleShadersPath", example_path / "shaders");

        m_camera_ubo = Mizu::UniformBuffer::create<CameraUBO>();
        m_render_finished_semaphore = Mizu::Semaphore::create();
        m_render_finished_fence = Mizu::Fence::create();

        const auto loader = Mizu::AssimpLoader::load(example_path / "cube.fbx");
        MIZU_ASSERT(loader.has_value(), "Could not load cube");

        m_cube_mesh = loader->get_meshes()[0];

        auto mesh_1 = m_scene->create_entity();
        mesh_1.add_component(Mizu::MeshRendererComponent{
            .mesh = m_cube_mesh,
        });
        mesh_1.get_component<Mizu::TransformComponent>().rotation = glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f);

        init(WIDTH, HEIGHT);
        m_presenter = Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_present_texture);
    }

    void on_update(double ts) override {
        m_time += static_cast<float>(ts);

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
    std::shared_ptr<Mizu::Cubemap> m_skybox;
    std::shared_ptr<Mizu::Mesh> m_cube_mesh;
    std::shared_ptr<Mizu::UniformBuffer> m_camera_ubo;
    std::shared_ptr<Mizu::Semaphore> m_render_finished_semaphore;
    std::shared_ptr<Mizu::Fence> m_render_finished_fence;

    Mizu::RenderGraph m_graph;

    float m_time = 0.0f;

    void init(uint32_t width, uint32_t height) {
        Mizu::RenderGraphBuilder builder;

        Mizu::ImageDescription texture_desc{};
        texture_desc.width = width;
        texture_desc.height = height;
        texture_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        texture_desc.usage = Mizu::ImageUsageBits::Attachment | Mizu::ImageUsageBits::Sampled;

        m_present_texture = Mizu::Texture2D::create(texture_desc);

        const auto skybox_path = std::filesystem::path(MIZU_EXAMPLE_PATH) / "skybox";

        Mizu::Cubemap::Faces faces;
        faces.right = (skybox_path / "right.jpg").string();
        faces.left = (skybox_path / "left.jpg").string();
        faces.top = (skybox_path / "top.jpg").string();
        faces.bottom = (skybox_path / "bottom.jpg").string();
        faces.front = (skybox_path / "front.jpg").string();
        faces.back = (skybox_path / "back.jpg").string();

        m_skybox = Mizu::Cubemap::create(faces);

        const Mizu::RGTextureRef present_texture_ref = builder.register_texture(m_present_texture);
        const Mizu::RGTextureRef depth_texture_ref =
            builder.create_texture(width, height, Mizu::ImageFormat::D32_SFLOAT);

        const Mizu::RGFramebufferRef framebuffer_ref =
            builder.create_framebuffer(width, height, {present_texture_ref, depth_texture_ref});

        const Mizu::RGCubemapRef skybox_ref = builder.register_cubemap(m_skybox);

        const Mizu::RGUniformBufferRef camera_ubo_ref = builder.register_uniform_buffer(m_camera_ubo);

        NormalShader::Parameters normal_pass_params;
        normal_pass_params.uCameraInfo = camera_ubo_ref;

        Mizu::RGGraphicsPipelineDescription pipeline_desc{};
        pipeline_desc.depth_stencil.depth_test = true;
        pipeline_desc.depth_stencil.depth_write = true;

        builder.add_pass<NormalShader>(
            "NormalPass",
            pipeline_desc,
            normal_pass_params,
            framebuffer_ref,
            [&](std::shared_ptr<Mizu::RenderCommandBuffer> command_buffer) {
                struct ModelInfoData {
                    glm::mat4 model;
                };

                for (const auto& entity : m_scene->view<Mizu::MeshRendererComponent>()) {
                    const Mizu::MeshRendererComponent& mesh_renderer =
                        entity.get_component<Mizu::MeshRendererComponent>();
                    const Mizu::TransformComponent& transform = entity.get_component<Mizu::TransformComponent>();

                    glm::mat4 model(1.0f);
                    model = glm::translate(model, transform.position);
                    model = glm::rotate(model, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                    model = glm::rotate(model, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::rotate(model, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                    model = glm::scale(model, transform.scale);

                    ModelInfoData model_info{};
                    model_info.model = model;
                    command_buffer->push_constant("uModelInfo", model_info);

                    command_buffer->draw_indexed(mesh_renderer.mesh->vertex_buffer(),
                                                 mesh_renderer.mesh->index_buffer());
                }
            });

        SkyboxShader::Parameters params{};
        params.uCameraInfo = camera_ubo_ref;
        params.uSkybox = skybox_ref;

        Mizu::RGGraphicsPipelineDescription skybox_pipeline_desc{};
        skybox_pipeline_desc.rasterization = Mizu::RasterizationState{
            .front_face = Mizu::RasterizationState::FrontFace::ClockWise,
        };
        skybox_pipeline_desc.depth_stencil.depth_compare_op = Mizu::DepthStencilState::DepthCompareOp::LessEqual;

        builder.add_pass<SkyboxShader>("SkyboxPass",
                                       skybox_pipeline_desc,
                                       params,
                                       framebuffer_ref,
                                       [&](std::shared_ptr<Mizu::RenderCommandBuffer> command_buffer) {
                                           struct ModelInfoData {
                                               glm::mat4 model;
                                           };

                                           auto model = glm::mat4(1.0f);
                                           model = glm::scale(model, glm::vec3(1.0f));

                                           const auto data = ModelInfoData{
                                               .model = model,
                                           };

                                           command_buffer->push_constant("uModelInfo", data);
                                           command_buffer->draw_indexed(m_cube_mesh->vertex_buffer(),
                                                                        m_cube_mesh->index_buffer());
                                       });

        auto graph = Mizu::RenderGraph::build(builder);
        assert(graph.has_value());

        m_graph = *graph;
    }
};

int main() {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Plasma";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
