#include "runtime/main_loop.h"

#include <thread>

#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/runtime.h"
#include "core/window.h"
#include "render/runtime/render_loop.h"
#include "render/runtime/renderer.h"

#include "runtime/game_main.h"
#include "runtime/simulation_loop.h"

namespace Mizu
{

MainLoop::~MainLoop()
{
    delete g_game_renderer;
    delete g_state_manager_coordinator;
    delete g_job_system;

    destroy_game_context();
}

bool MainLoop::init()
{
    // Init Logging
    MIZU_LOG_SETUP;

    // Init JobSystem
    const uint32_t num_threads = std::thread::hardware_concurrency();
    MIZU_VERIFY(num_threads >= 4, "At least 4 threads are required to run the engine");

    constexpr uint32_t JOB_SYSTEM_THREADS = 4;

    g_job_system = new JobSystem{};
    g_job_system->init(JOB_SYSTEM_THREADS);

    // Init StateManager
    g_state_manager_coordinator = new StateManagerCoordinator{};

    // Create Game Main
    m_game_main = create_game_main();
    MIZU_ASSERT(m_game_main != nullptr, "GameMain is nullptr");

    const GameDescription& game_desc = m_game_main->get_game_description();

    m_window = std::make_shared<Window>(game_desc.name, game_desc.width, game_desc.height, game_desc.graphics_api);
    create_game_context(m_window);

    // Init Renderer
    init_renderer(game_desc);

    // Init Simulation
    init_simulation();

    return true;
}

void MainLoop::init_renderer(const GameDescription& desc)
{
    GameRendererDescription renderer_desc{};
    renderer_desc.graphics_api = desc.graphics_api;
    renderer_desc.window = m_window;
    renderer_desc.application_name = desc.name;
    renderer_desc.application_version = desc.version;

    g_game_renderer = new GameRenderer{renderer_desc};
    m_game_main->setup_game_renderer(*g_game_renderer);
}

void MainLoop::init_simulation()
{
    m_game_simulation = m_game_main->create_game_simulation();
    MIZU_ASSERT(m_game_simulation != nullptr, "GameSimulation is nullptr");

    m_window->add_event_callback_func([&](Event& event) {
        switch (event.get_type())
        {
        case EventType::WindowResized: {
            auto& window_resized = static_cast<WindowResizedEvent&>(event);
            m_game_simulation->on_window_resized(window_resized);
            break;
        }
        case EventType::MouseMoved: {
            auto& mouse_moved = static_cast<MouseMovedEvent&>(event);
            m_game_simulation->on_mouse_moved(mouse_moved);
            break;
        }
        case EventType::MouseButtonPressed: {
            auto& mouse_pressed = static_cast<MousePressedEvent&>(event);
            m_game_simulation->on_mouse_pressed(mouse_pressed);
            break;
        }
        case EventType::MouseButtonReleased: {
            auto& mouse_released = static_cast<MouseReleasedEvent&>(event);
            m_game_simulation->on_mouse_released(mouse_released);
            break;
        }
        case EventType::MouseScrolled: {
            auto& mouse_scrolled = static_cast<MouseScrolledEvent&>(event);
            m_game_simulation->on_mouse_scrolled(mouse_scrolled);
            break;
        }
        case EventType::KeyPressed: {
            auto& key_pressed = static_cast<KeyPressedEvent&>(event);
            m_game_simulation->on_key_pressed(key_pressed);
            break;
        }
        case EventType::KeyReleased: {
            auto& key_released = static_cast<KeyReleasedEvent&>(event);
            m_game_simulation->on_key_released(key_released);
            break;
        }
        case EventType::KeyRepeated: {
            auto& key_repeat = static_cast<KeyRepeatEvent&>(event);
            m_game_simulation->on_key_repeat(key_repeat);
            break;
        }
        }
    });
}

void MainLoop::run()
{
    SimulationLoop simulation_loop{*m_game_simulation, &MainLoop::shutdown_job};
    RenderLoop render_loop{*g_game_renderer, &MainLoop::shutdown_job};

    run_multi_threaded(simulation_loop, render_loop);
}

void MainLoop::run_multi_threaded(SimulationLoop& simulation_loop, RenderLoop& render_loop)
{
    // 2 = simulation + rendering shutdown jobs
    m_shutdown_counter.store(2);

    simulation_loop.init();

    simulation_loop.create_update_job();
    render_loop.create_update_jobs();

    g_job_system->attach_as_main_worker();

    g_job_system->wait_workers_dead();
}

void MainLoop::shutdown_job()
{
    MIZU_PROFILE_SCOPED;

    if (m_shutdown_counter.fetch_sub(1, std::memory_order_seq_cst) == 1)
    {
        g_job_system->kill();
    }
}

} // namespace Mizu
