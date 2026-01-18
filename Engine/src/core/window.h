#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

#include "core/events.h"
#include "render_core/definitions/rhi_window.h"

// Forward declarations
struct GLFWwindow;

namespace Mizu
{

// Forward declarations
enum class GraphicsApi;

class Window : public IRHIWindow
{
  public:
    Window(std::string_view title, uint32_t width, uint32_t height, GraphicsApi graphics_api);
    ~Window() override;

    void update();
    void poll_events();
    void swap_buffers() const;

    bool should_close() const;
    double get_current_time() const;

    void add_event_callback_func(std::function<void(Event&)> func);

#if MIZU_RENDER_CORE_VULKAN_ENABLED
    static std::vector<const char*> get_vulkan_instance_extensions();
    void create_vulkan_surface(VkInstance_T* instance, VkSurfaceKHR_T*& surface) const override;
#endif

#if MIZU_RENDER_CORE_DX12_ENABLED
    void* create_dx12_window_handle() const override;
#endif

    uint32_t get_width() const override { return m_data.width; }
    uint32_t get_height() const override { return m_data.height; }
    glm::vec2 get_mouse_change() const { return m_data.mouse_change; }
    glm::vec2 get_mouse_position() const { return m_data.mouse_position; }
    glm::vec2 get_scroll_change() const { return m_data.scroll_change; }

    GLFWwindow* handle() const { return m_window; }

  private:
    GLFWwindow* m_window;
    GraphicsApi m_graphics_api;

    struct WindowData
    {
        uint32_t width;
        uint32_t height;

        glm::vec2 mouse_position;
        glm::vec2 mouse_change;

        glm::vec2 scroll_change;

        std::function<void(Event&)> event_callback;
    };

    WindowData m_data{};
    std::vector<std::function<void(Event&)>> m_event_callback_funcs;

    void on_event(Event& event);
};

} // namespace Mizu