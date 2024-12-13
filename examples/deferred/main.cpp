#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>

#include <random>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class ExampleLayer : public Mizu::Layer
{
  public:
    ExampleLayer()
    {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera_controller =
            std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 2.0f, 4.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);

        const auto mesh_path = example_path / "cube.fbx";

        auto loader = Mizu::AssimpLoader::load(mesh_path);
        MIZU_ASSERT(loader.has_value(), "Could not load mesh");

        std::random_device s_random_device;
        std::mt19937 s_rng(s_random_device());
        std::uniform_real_distribution<float> s_distribution(-10.0f, 10.0f);

        for (uint32_t i = 0; i < 10; ++i)
        {
            const float x = s_distribution(s_rng);
            const float z = s_distribution(s_rng);

            auto entity = m_scene->create_entity();
            entity.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
            });

            entity.get_component<Mizu::TransformComponent>().position = glm::vec3(x, 0.0f, z);
            entity.get_component<Mizu::TransformComponent>().rotation = glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f);
        }

        m_renderer = std::make_unique<Mizu::DeferredRenderer>(m_scene, WIDTH, HEIGHT);
        m_presenter =
            Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_renderer->get_result_texture());
    }

    ~ExampleLayer() { Mizu::Renderer::wait_idle(); }

    void on_update(double ts) override
    {
        m_camera_controller->update(ts);

        m_renderer->render(*m_camera_controller);

        m_presenter->present(m_renderer->get_render_semaphore());
    }

    void on_window_resized(Mizu::WindowResizeEvent& event) override
    {
        Mizu::Renderer::wait_idle();
        m_renderer->resize(event.get_width(), event.get_height());
        m_presenter->texture_changed(m_renderer->get_result_texture());
        m_camera_controller->set_aspect_ratio(static_cast<float>(event.get_width())
                                              / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;
    std::shared_ptr<Mizu::Presenter> m_presenter;
    std::unique_ptr<Mizu::ISceneRenderer> m_renderer;
};

int main()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Deferred Renderer Example";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
