#include "Mizu/Mizu.h"

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {
        auto texture = Mizu::Texture2D::create(Mizu::ImageDescription{
            .width = 1920,
            .height = 1080,
        });
        assert(texture != nullptr);

        m_presenter = Mizu::Presenter::create(Mizu::Application::instance()->get_window(), texture);
    }

    void on_update(double ts) override {}

  private:
    std::shared_ptr<Mizu::Presenter> m_presenter;
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
