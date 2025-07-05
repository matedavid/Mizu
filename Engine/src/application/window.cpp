#include "window.h"

#include <GLFW/glfw3.h>
#include <memory>

#include "base/debug/logging.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

Window::Window(std::string_view title, uint32_t width, uint32_t height, GraphicsAPI graphics_api)
    : m_graphics_api(graphics_api)
{
    const int result = glfwInit();
    MIZU_VERIFY(result, "Failed to initialize GLFW");

    switch (graphics_api)
    {
    case GraphicsAPI::Vulkan: {
        if (!glfwVulkanSupported())
        {
            glfwTerminate();
            MIZU_UNREACHABLE("GLFW does not support Vulkan");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    break;
    case GraphicsAPI::OpenGL: {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    }
    break;
    }

    m_window =
        glfwCreateWindow(static_cast<int32_t>(width), static_cast<int32_t>(height), title.data(), nullptr, nullptr);

    if (m_graphics_api == GraphicsAPI::OpenGL)
    {
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1); // Enable vsync
    }

    int32_t framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);

    m_data.width = static_cast<uint32_t>(framebuffer_width);
    m_data.height = static_cast<uint32_t>(framebuffer_height);
    m_data.mouse_position = glm::vec2(0.0f);
    m_data.mouse_change = glm::vec2(0.0f);
    m_data.event_callback = [&](Event& event) {
        on_event(event);
    };

    glfwSetWindowUserPointer(m_window, &m_data);

    //
    // Window events
    //

    // Window resize event
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int32_t w, int32_t h) {
        auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        data->width = static_cast<uint32_t>(w);
        data->height = static_cast<uint32_t>(h);

        WindowResizedEvent event(data->width, data->height);
        data->event_callback(event);
    });

    // Mouse events
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
        auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        const glm::vec2 pos = glm::vec2(xpos, ypos);
        data->mouse_change = pos - data->mouse_position;
        data->mouse_position = pos;

        MouseMovedEvent event(xpos, ypos, data->mouse_change);
        data->event_callback(event);
    });

    // Mouse button events
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int32_t button, int32_t action, int32_t mods) {
        const auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        const MouseButton mouse_button = static_cast<MouseButton>(button);
        const ModifierKeyBits mods_bits = static_cast<ModifierKeyBits>(mods);

        if (action == GLFW_PRESS)
        {
            MousePressedEvent event(mouse_button, mods_bits);
            data->event_callback(event);
        }
        else if (action == GLFW_RELEASE)
        {
            MouseReleasedEvent event(mouse_button, mods_bits);
            data->event_callback(event);
        }
    });

    // Scroll event
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
        const auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        MouseScrolledEvent event(xoffset, yoffset);
        data->event_callback(event);
    });

    // Keyboard events
    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int32_t _key, int32_t scancode, int32_t action, int32_t mods) {
        const auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        const Key key = static_cast<Key>(_key);
        const ModifierKeyBits mods_bits = static_cast<ModifierKeyBits>(mods);

        if (action == GLFW_PRESS)
        {
            KeyPressedEvent event(key, scancode, mods_bits);
            data->event_callback(event);
        }
        else if (action == GLFW_RELEASE)
        {
            KeyReleasedEvent event(key, scancode, mods_bits);
            data->event_callback(event);
        }
        else if (action == GLFW_REPEAT)
        {
            KeyRepeatEvent event(key, scancode, mods_bits);
            data->event_callback(event);
        }
    });
}

Window::~Window()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Window::update()
{
    m_data.mouse_change = glm::vec2(0.0f);
    glfwPollEvents();

    if (m_graphics_api == GraphicsAPI::OpenGL)
    {
        glfwSwapBuffers(m_window);
    }
}

void Window::poll_events()
{
    m_data.mouse_change = glm::vec2(0.0f);
    glfwPollEvents();
}

void Window::swap_buffers() const
{
    if (m_graphics_api == GraphicsAPI::OpenGL)
    {
        glfwSwapBuffers(m_window);
    }
}

bool Window::should_close() const
{
    return glfwWindowShouldClose(m_window);
}

double Window::get_current_time() const
{
    return glfwGetTime();
}

void Window::add_event_callback_func(std::function<void(Event&)> func)
{
    m_event_callback_funcs.push_back(std::move(func));
}

std::vector<const char*> Window::get_vulkan_instance_extensions()
{
    uint32_t number_extensions = 0;
    const auto& glfw_extensions = glfwGetRequiredInstanceExtensions(&number_extensions);

    std::vector<const char*> extensions{};
    for (uint32_t i = 0; i < number_extensions; ++i)
    {
        extensions.push_back(glfw_extensions[i]);
    }

    return extensions;
}

VkResult Window::create_vulkan_surface(const VkInstance& instance, VkSurfaceKHR& surface) const
{
    return glfwCreateWindowSurface(instance, m_window, nullptr, &surface);
}

void Window::on_event(Event& event)
{
    for (const auto& func : m_event_callback_funcs)
    {
        func(event);
    }
}

} // namespace Mizu
