#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/layer.h"
#include "input/events.h"
#include "renderer/abstraction/renderer.h"

namespace Mizu {

// Forward declarations
class Window;

class Application {
  public:
    struct Description {
        std::string name = "Mizu Application";
        Version version = Version{0, 1, 0};
        GraphicsAPI graphics_api = GraphicsAPI::Vulkan;

        uint32_t width = 1280;
        uint32_t height = 720;
    };

    explicit Application(Description description);
    ~Application();

    void run();

    template <typename T, typename... Args>
    void push_layer(Args... args) {
        static_assert(std::is_base_of<Layer, T>());
        m_layers.push_back(std::make_unique<T>(args...));
    }

    [[nodiscard]] const std::shared_ptr<Window>& get_window() const { return m_window; }
    static Application* instance();

  private:
    Description m_description;
    std::shared_ptr<Window> m_window;

    std::vector<std::unique_ptr<Layer>> m_layers;

    static Application* s_instance;

    void on_event(Event& event);
};

} // namespace Mizu
