#include "application.h"

#include <ranges>

#include "application/window.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

Application::Application(Description description) : m_description(std::move(description))
{
    MIZU_VERIFY(s_instance == nullptr, "Can only have one instance of Application");

    MIZU_LOG_SETUP;

    m_window = std::make_shared<Window>(
        m_description.name, m_description.width, m_description.height, m_description.graphics_api);
    m_window->add_event_callback_func([&](Event& event) { on_event(event); });

    BackendSpecificConfiguration backend_config;
    switch (m_description.graphics_api)
    {
    case GraphicsAPI::Vulkan:
        backend_config = VulkanSpecificConfiguration{
            .instance_extensions = m_window->get_vulkan_instance_extensions(),
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

    MIZU_VERIFY(Renderer::initialize(config), "Failed to initialize Renderer");

    s_instance = this;
}

Application::~Application()
{
    m_layers.clear();

    Renderer::shutdown();

    s_instance = nullptr;
}

void Application::run()
{
    if (m_layers.empty())
    {
        MIZU_LOG_WARNING("No layer has been added to Application");
    }

    on_init();

    double last_time = m_window->get_current_time();

    while (!m_window->should_close())
    {
        const double current_time = m_window->get_current_time();
        const double ts = current_time - last_time;
        last_time = current_time;

        on_update(ts);

        m_window->update();
    }
}

void Application::on_init()
{
    for (auto& layer : m_layers)
    {
        layer->on_init();
    }
}

void Application::on_update(double ts)
{
    for (auto& layer : m_layers)
    {
        layer->on_update(ts);
    }
}

void Application::on_event(Event& event)
{
    for (auto& layer : std::views::reverse(m_layers))
    {
        if (event.handled)
            break;

        switch (event.get_type())
        {
        default:
        case EventType::WindowResized: {
            auto& window_resized = dynamic_cast<WindowResizedEvent&>(event);
            layer->on_window_resized(window_resized);
            break;
        }
        case EventType::MouseMoved: {
            auto& mouse_moved = dynamic_cast<MouseMovedEvent&>(event);
            layer->on_mouse_moved(mouse_moved);
            break;
        }
        case EventType::MouseButtonPressed: {
            auto& mouse_pressed = dynamic_cast<MousePressedEvent&>(event);
            layer->on_mouse_pressed(mouse_pressed);
            break;
        }
        case EventType::MouseButtonReleased: {
            auto& mouse_released = dynamic_cast<MouseReleasedEvent&>(event);
            layer->on_mouse_released(mouse_released);
            break;
        }
        case EventType::MouseScrolled: {
            auto& mouse_scrolled = dynamic_cast<MouseScrolledEvent&>(event);
            layer->on_mouse_scrolled(mouse_scrolled);
            break;
        }
        case EventType::KeyPressed: {
            auto& key_pressed = dynamic_cast<KeyPressedEvent&>(event);
            layer->on_key_pressed(key_pressed);
            break;
        }
        case EventType::KeyReleased: {
            auto& key_released = dynamic_cast<KeyReleasedEvent&>(event);
            layer->on_key_released(key_released);
            break;
        }
        case EventType::KeyRepeated: {
            auto& key_repeat = dynamic_cast<KeyRepeatEvent&>(event);
            layer->on_key_repeat(key_repeat);
            break;
        }
        }
    }
}

Application* Application::s_instance = nullptr;

Application* Application::instance()
{
    MIZU_VERIFY(s_instance != nullptr, "Application has not been created");
    return s_instance;
}

} // namespace Mizu
