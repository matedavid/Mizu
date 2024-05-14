#include "Mizu/Mizu.h"

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {}

    void on_update(double ts) override {}
};

int main() {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Example";

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
