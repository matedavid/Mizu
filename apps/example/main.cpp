#include <Mizu/Mizu.h>
#include <Mizu/Plugins/CameraControllers.h>

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera_controller =
            std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 1.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        auto cube_1 = m_scene->create_entity();
        cube_1.add_component(Mizu::MeshRendererComponent{
            .mesh = Mizu::PrimitiveFactory::get_pyramid(),
        });

        init(WIDTH, HEIGHT);
        m_presenter =
            Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_renderer->output_texture());
    }

    void on_update(double ts) override {
        m_camera_controller->update(ts);
        m_renderer->render(*m_camera_controller);
        m_presenter->present(m_renderer->render_finished_semaphore());
    }

    void on_window_resized(Mizu::WindowResizeEvent& event) override {
        Mizu::Renderer::wait_idle();
        init(event.get_width(), event.get_height());
        m_presenter->texture_changed(m_renderer->output_texture());
        m_camera_controller->set_aspect_ratio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;
    std::shared_ptr<Mizu::Presenter> m_presenter;

    std::unique_ptr<Mizu::ISceneRenderer> m_renderer;

    void init(uint32_t width, uint32_t height) {
        m_renderer = std::make_unique<Mizu::ForwardRenderer>(m_scene, width, height);
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
