#include <Mizu/Mizu.h>

#include <thread>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class ExampleLayer : public Mizu::Layer
{
  public:
    void on_update(double ts) override { MIZU_LOG_INFO("ts is: {}", ts); }
};

Mizu::Application* Mizu::create_application()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "State Manager Example";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application* app = new Mizu::Application(desc);
    app->push_layer<ExampleLayer>();

    return app;
}

/*
int main()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "State Manager Example";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
*/
