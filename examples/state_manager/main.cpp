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
    void on_update(double ts) override { MIZU_LOG_INFO("ts is: {}", ts); }

    void on_window_resized(WindowResizedEvent& event) override
    {
        MIZU_LOG_INFO("WindowResized: {} {}", event.get_width(), event.get_height());
    }
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
