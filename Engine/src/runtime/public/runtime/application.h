#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/events.h"
#include "render/renderer.h"

#include "mizu_runtime_module.h"
#include "runtime/layer.h"

namespace Mizu
{

// Forward declarations
class Window;

class MIZU_RUNTIME_API Application
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

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Application(Application&&) = default;
    Application& operator=(Application&&) = default;

    // void run();

    void on_init();
    void on_update(double ts);
    void on_event(Event& event);

    template <typename T, typename... Args>
    void push_layer(Args... args)
    {
        static_assert(std::is_base_of<Layer, T>(), "Layer must inherit from Layer");

        m_layers.push_back(std::make_unique<T>(args...));
    }

    const Application::Description& get_description() const { return m_description; }
    static Application* instance();

  private:
    Description m_description;
    std::vector<std::unique_ptr<Layer>> m_layers;

    static Application* s_instance;
};

} // namespace Mizu
