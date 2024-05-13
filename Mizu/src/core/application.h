#pragma once

#include <string>

#include "input/events.h"
#include "renderer/abstraction/renderer.h"

namespace Mizu {

// Forward declarations
class Window;

class Application {
  public:
    struct Description {
        std::string name = "";
        Version version = Version{0, 1, 0};
        GraphicsAPI graphics_api = GraphicsAPI::Vulkan;

        uint32_t width = 1280;
        uint32_t height = 720;
    };

    explicit Application(Description description);
    ~Application();

    void run();

    [[nodiscard]] const std::unique_ptr<Window>& get_window() const { return m_window; }
    static Application* instance();

  private:
    Description m_description;
    std::unique_ptr<Window> m_window;

    static Application* s_instance;

    void on_event(Event& event);
};

} // namespace Mizu