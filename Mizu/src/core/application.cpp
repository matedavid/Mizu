#include "application.h"

#include <ranges>

#include "core/window.h"
#include "utility/logging.h"

namespace Mizu {

Application::Application(Description description) : m_description(std::move(description)) {
    m_window = std::make_unique<Window>(
        m_description.name, m_description.width, m_description.height, m_description.graphics_api);
    m_window->add_event_callback_func([&](Event& event) { on_event(event); });

    BackendSpecificConfiguration backend_config;
    switch (m_description.graphics_api) {
    case GraphicsAPI::Vulkan:
        backend_config = VulkanSpecificConfiguration{
            .instance_extensions = {}, // TODO:
        };
        break;
    case GraphicsAPI::OpenGL:
        backend_config = OpenGLSpecificConfiguration{};
        break;
    }

    RendererConfiguration config{};
    config.graphics_api = m_description.graphics_api;
    config.backend_specific_config = backend_config;
    config.application_name = m_description.name;
    config.application_version = m_description.version;

    [[maybe_unused]] bool initialized = Renderer::initialize(config);
    assert(initialized && "Failed to initialize renderer");

    s_instance = this;
}

Application::~Application() {
    Renderer::shutdown();
}

void Application::run() {
    while (!m_window->should_close()) {
        m_window->update();
    }
}

void Application::on_event(Event& event) {
    for (auto& layer : std::views::reverse(m_layers)) {
        if (event.handled)
            break;

        switch (event.get_type()) {
        default:
        case EventType::WindowResize: {
            auto window_resized = dynamic_cast<WindowResizeEvent&>(event);
            layer->on_window_resized(window_resized);
            break;
        }
        case EventType::MouseMoved: {
            auto mouse_moved = dynamic_cast<MouseMovedEvent&>(event);
            layer->on_mouse_moved(mouse_moved);
            break;
        }
        case EventType::MouseButtonPressed: {
            auto mouse_pressed = dynamic_cast<MousePressedEvent&>(event);
            layer->on_mouse_pressed(mouse_pressed);
            break;
        }
        case EventType::MouseButtonReleased: {
            auto mouse_released = dynamic_cast<MouseReleasedEvent&>(event);
            layer->on_mouse_released(mouse_released);
            break;
        }
        case EventType::MouseScrolled: {
            auto mouse_scrolled = dynamic_cast<MouseScrolledEvent&>(event);
            layer->on_mouse_scrolled(mouse_scrolled);
            break;
        }
        case EventType::KeyPressed: {
            auto key_pressed = dynamic_cast<KeyPressedEvent&>(event);
            layer->on_key_pressed(key_pressed);
            break;
        }
        case EventType::KeyReleased: {
            auto key_released = dynamic_cast<KeyReleasedEvent&>(event);
            layer->on_key_released(key_released);
            break;
        }
        case EventType::KeyRepeated: {
            auto key_repeat = dynamic_cast<KeyRepeatEvent&>(event);
            layer->on_key_repeat(key_repeat);
            break;
        }
        }
    }
}

Application* Application::s_instance = nullptr;

Application* Application::instance() {
    assert(s_instance != nullptr);
    return s_instance;
}

} // namespace Mizu
