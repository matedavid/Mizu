#include "Mizu/Mizu.h"

#include "plugins/camera_controllers/first_person_camera_controller.h"

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        // m_camera = std::make_shared<Mizu::PerspectiveCamera>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        // m_camera->set_position({0.0f, 0.0f, 1.0f});

        m_camera_controller = std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 1.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        auto cube_1 = m_scene->create_entity();
        cube_1.add_component(Mizu::MeshRendererComponent{
            .mesh = Mizu::PrimitiveFactory::get_cube(),
        });

        init(WIDTH, HEIGHT);
        m_presenter =
            Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_renderer->output_texture());
    }

    void on_update(double ts) override {
        m_camera_controller->update(ts);
        m_renderer->render(*m_camera_controller);
        m_presenter->present();
    }

    void on_window_resized(Mizu::WindowResizeEvent& event) override {
        Mizu::Renderer::wait_idle();
        init(event.get_width(), event.get_height());
        m_presenter->texture_changed(m_renderer->output_texture());
        // m_camera->set_aspect_ratio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
        m_camera_controller->set_aspect_ratio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
    }

    void on_key_pressed(Mizu::KeyPressedEvent& event) override {
        /*
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

        if (event.get_key() == Mizu::Key::Q) {
            pos.y -= 1.0f;
        }

        if (event.get_key() == Mizu::Key::E) {
            pos.y += 1.0f;
        }

        m_camera->set_position(pos);
        */
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::shared_ptr<Mizu::PerspectiveCamera> m_camera;
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
