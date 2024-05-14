#include "Mizu/Mizu.h"

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {}

    void on_update(double ts) override {}
};

Mizu::Application* create_application(int argc, const char* argv[]) {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Example";

    Mizu::Application* app = new Mizu::Application(desc);
    app->push_layer<ExampleLayer>();

    return app;
}