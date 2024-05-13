#include "Mizu/Mizu.h"

Mizu::Application* create_application(int argc, const char* argv[]) {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Example";

    Mizu::Application* app = new Mizu::Application(desc);
    return app;
}