#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include "input/events.h"

// Forward declarations
struct GLFWwindow;

namespace Mizu
{

// Forward declarations
enum class GraphicsAPI;

class Window
{
  public:
    Window(std::string_view title, uint32_t width, uint32_t height, GraphicsAPI graphics_api);
    ~Window();

    void update();
    [[nodiscard]] bool should_close() const;
    [[nodiscard]] double get_current_time() const;

    void add_event_callback_func(std::function<void(Event&)> func);

    [[nodiscard]] static std::vector<const char*> get_vulkan_instance_extensions();
    VkResult create_vulkan_surface(const VkInstance& instance, VkSurfaceKHR& surface) const;

    [[nodiscard]] uint32_t get_width() const { return m_data.width; }
    [[nodiscard]] uint32_t get_height() const { return m_data.height; }
    [[nodiscard]] glm::vec2 get_mouse_change() const { return m_data.mouse_change; }
    [[nodiscard]] glm::vec2 get_mouse_position() const { return m_data.mouse_position; }

    [[nodiscard]] GLFWwindow* handle() const { return m_window; }

  private:
    GLFWwindow* m_window;
    GraphicsAPI m_graphics_api;

    struct WindowData
    {
        uint32_t width;
        uint32_t height;

        glm::vec2 mouse_position;
        glm::vec2 mouse_change;

        std::function<void(Event&)> event_callback;
    };

    WindowData m_data{};
    std::vector<std::function<void(Event&)>> m_event_callback_funcs;

    void on_event(Event& event);
};

} // namespace Mizu