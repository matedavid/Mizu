#include <Mizu/Mizu.h>

class PruebasLayer : public Mizu::Layer {
  public:
    Mizu::RenderGraph graph;

    PruebasLayer() {
        auto tex = Mizu::Texture2D::create({.width = 1920, .height = 1080, .usage = Mizu::ImageUsageBits::Sampled});

        graph.register_texture(tex);
        graph.add_pass("Example", Mizu::GraphicsPipeline::Description{}, [&](auto cb) { render_models(cb); });
    }

    void on_update(double ts) { graph.execute(); }

    void render_models(std::shared_ptr<Mizu::ICommandBuffer> command_buffer) {
        // Do something
    }
};

int main() {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Example";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application app(desc);
    app.push_layer<PruebasLayer>();

    app.run();
}