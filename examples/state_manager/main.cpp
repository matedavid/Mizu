#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Mizu.h>

#include <thread>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

using namespace Mizu;

class ExampleLayer : public Layer
{
  public:
    ExampleLayer() {}

    void on_update(double ts) override
    {
        m_camera_controller.update(ts);

        CameraDynamicState camera_dyn_state{};
        camera_dyn_state.view = m_camera_controller.view_matrix();
        camera_dyn_state.proj = m_camera_controller.projection_matrix();
        camera_dyn_state.pos = m_camera_controller.get_position();
        sim_set_camera_state(camera_dyn_state);
    }

    void on_window_resized(WindowResizedEvent& event) override
    {
        MIZU_LOG_INFO("WindowResized: {} {}", event.get_width(), event.get_height());
    }

  private:
    FirstPersonCameraController m_camera_controller;
};

Application* Mizu::create_application()
{
    Application::Description desc{};
    desc.graphics_api = GraphicsAPI::Vulkan;
    desc.name = "State Manager Example";
    desc.width = 1920;
    desc.height = 1080;

    Application* app = new Application(desc);
    app->push_layer<ExampleLayer>();

    return app;
}
