#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "application/layer.h"
#include "input/events.h"
#include "renderer/renderer.h"

namespace Mizu
{

// Forward declarations
class Window;

class Application
{
  public:
    struct Description
    {
        std::string name = "Mizu Application";
        Version version = Version{0, 1, 0};
        GraphicsApi graphics_api = GraphicsApi::Vulkan;

        uint32_t width = 1280;
        uint32_t height = 720;
    };

    explicit Application(Description description);
    ~Application();

    void run();

    void on_init();
    void on_update(double ts);

    template <typename T, typename... Args>
    void push_layer(Args... args)
    {
        static_assert(std::is_base_of<Layer, T>(), "Layer must inherit from Layer");

        m_layers.push_back(std::make_unique<T>(args...));
    }

    const Application::Description& get_description() const { return m_description; }
    const std::shared_ptr<Window>& get_window() const { return m_window; }
    static Application* instance();

  private:
    Description m_description;
    std::shared_ptr<Window> m_window;

    std::vector<std::unique_ptr<Layer>> m_layers;

    static Application* s_instance;

    void on_event(Event& event);
};

} // namespace Mizu
