#include "window.h"

#include <GLFW/glfw3.h>
#include <memory>

#include "utility/logging.h"

#include "renderer/abstraction/renderer.h"

namespace Mizu {

Window::Window(std::string_view title, uint32_t width, uint32_t height, GraphicsAPI graphics_api)
      : m_graphics_api(graphics_api) {
    [[maybe_unused]] const auto result = glfwInit();
    assert(result && "Failed to initialize GLFW");

    switch (graphics_api) {
    case GraphicsAPI::Vulkan: {
        if (!glfwVulkanSupported()) {
            glfwTerminate();
            assert(false && "GLFW does not support vulkan");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    } break;
    case GraphicsAPI::OpenGL: {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    } break;
    }

    m_window =
        glfwCreateWindow(static_cast<int32_t>(width), static_cast<int32_t>(height), title.data(), nullptr, nullptr);

    if (m_graphics_api == GraphicsAPI::OpenGL) {
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1); // Enable vsync
    }

    m_data.width = width;
    m_data.height = height;
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

        auto event = WindowResizeEvent(data->width, data->height);
        data->event_callback(event);
    });

    // Mouse events
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
        auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        const auto pos = glm::vec2(xpos, ypos);
        data->mouse_change = pos - data->mouse_position;
        data->mouse_position = pos;

        auto event = MouseMovedEvent(xpos, ypos);
        data->event_callback(event);
    });

    // Mouse button events
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int32_t button, int32_t action, int32_t mods) {
        const auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        const auto mouse_button = static_cast<MouseButton>(button);

        if (action == GLFW_PRESS) {
            auto event = MousePressedEvent(mouse_button, mods);
            data->event_callback(event);
        } else if (action == GLFW_RELEASE) {
            auto event = MouseReleasedEvent(mouse_button, mods);
            data->event_callback(event);
        }
    });

    // Scroll event
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
        const auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        auto event = MouseScrolledEvent(xoffset, yoffset);
        data->event_callback(event);
    });

    // Keyboard events
    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int32_t _key, int32_t scancode, int32_t action, int32_t mods) {
        const auto* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

        const auto key = static_cast<Key>(_key);
        if (action == GLFW_PRESS) {
            auto event = KeyPressedEvent(key, scancode, mods);
            data->event_callback(event);
        } else if (action == GLFW_RELEASE) {
            auto event = KeyReleasedEvent(key, scancode, mods);
            data->event_callback(event);
        } else if (action == GLFW_REPEAT) {
            auto event = KeyRepeatEvent(key, scancode, mods);
            data->event_callback(event);
        }
    });
}

Window::~Window() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Window::update() {
    m_data.mouse_change = glm::vec2(0.0f);
    glfwPollEvents();

    if (m_graphics_api == GraphicsAPI::OpenGL) {
        glfwSwapBuffers(m_window);
    }
}

bool Window::should_close() const {
    return glfwWindowShouldClose(m_window);
}

double Window::get_current_time() const {
    return glfwGetTime();
}

void Window::add_event_callback_func(std::function<void(Event&)> func) {
    m_event_callback_funcs.push_back(std::move(func));
}

std::vector<const char*> Window::get_vulkan_instance_extensions() {
    uint32_t number_extensions = 0;
    const auto& glfw_extensions = glfwGetRequiredInstanceExtensions(&number_extensions);

    std::vector<const char*> extensions{};
    for (uint32_t i = 0; i < number_extensions; ++i) {
        extensions.push_back(glfw_extensions[i]);
    }

    return extensions;
}

VkResult Window::create_surface(const VkInstance& instance, VkSurfaceKHR& surface) const {
    return glfwCreateWindowSurface(instance, m_window, nullptr, &surface);
}

void Window::on_event(Event& event) {
    for (const auto& func : m_event_callback_funcs) {
        func(event);
    }
}

} // namespace Mizu