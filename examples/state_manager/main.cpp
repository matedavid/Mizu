#include <Mizu/Mizu.h>

#include <thread>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

#define g_transform_manager Mizu::g_transform_manager

Mizu::TransformHandle g_handle;
Mizu::TransformHandle g_handle2;
bool initialized = false;
uint64_t i = 0;

static void simulation_loop()
{
    while (true)
    {
        g_transform_manager->sim_begin_tick();

        if (!initialized)
        {
            Mizu::TransformStaticState static_state{};
            Mizu::TransformDynamicState dynamic_state{};

            g_handle = g_transform_manager->sim_create_handle(static_state, dynamic_state);
            g_handle2 = g_transform_manager->sim_create_handle(static_state, dynamic_state);
            initialized = true;
        }

        constexpr uint64_t num = 100;
        if (i % num == 0)
        {
            Mizu::TransformDynamicState dynamic_state{};
            dynamic_state.translation = glm::vec3(float(i) / float(num));

            g_transform_manager->sim_update(g_handle, dynamic_state);
        }

        if ((i + 1) % 1000 == 0)
        {
            g_transform_manager->sim_release_handle(g_handle2);
            g_handle2 = g_transform_manager->sim_create_handle({}, {});
        }

        g_transform_manager->sim_end_tick();
        i += 1;

        // std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(33.3f));
    }
}

static void rendering_loop()
{
    while (true)
    {
        g_transform_manager->rend_begin_frame();

        if (i > 2)
        {
            Mizu::TransformDynamicState dyn = g_transform_manager->rend_get_dynamic_state(g_handle);
            MIZU_LOG_INFO("{}, {}, {}", dyn.translation.x, dyn.translation.y, dyn.translation.z);
        }

        g_transform_manager->rend_end_frame();

        // std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(16.6f));
    }
}

class ExampleLayer : public Mizu::Layer
{
  public:
    ExampleLayer()
    {
        g_transform_manager = new Mizu::TransformStateManager();

        std::thread sim_thread(simulation_loop);
        std::thread rend_thread(rendering_loop);

        sim_thread.join();
        rend_thread.join();
    }

    ~ExampleLayer() override {}

    void on_update(double ts) override {}
};

int main()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "State Manager Example";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
