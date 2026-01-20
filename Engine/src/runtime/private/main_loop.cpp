#include "runtime/main_loop.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/thread_sync.h"
#include "core/window.h"
#include "render/scene_renderer.h"
#include "render/shader/shader_manager.h"
#include "render/state_manager/state_manager_coordinator.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "state_manager/base_state_manager.h"

#include "runtime/application.h"

namespace Mizu
{

MainLoop::MainLoop() : m_application(nullptr) {}

MainLoop::~MainLoop()
{
    ShaderManager::get().reset();
    PipelineCache::get().reset();
    SamplerStateCache::get().reset();

    delete g_render_device;

    destroy_game_context();
    delete g_job_system;
    delete m_application;
}

bool MainLoop::init()
{
    // Init Logging
    MIZU_LOG_SETUP;

    // Init JobSystem
    const uint32_t num_threads = std::thread::hardware_concurrency();
    MIZU_VERIFY(num_threads >= 4, "At least 4 threads are required to run the engine");

    constexpr uint32_t JOB_SYSTEM_THREADS = 4; // num_threads - 1
    constexpr size_t JOB_SYSTEM_CAPACITY = 256;

    g_job_system = new JobSystem(JOB_SYSTEM_THREADS, JOB_SYSTEM_CAPACITY);
    g_job_system->init();

    // Init Application
    m_application = create_application();

    const Application::Description& desc = m_application->get_description();

    m_window = std::make_shared<Window>(desc.name, desc.width, desc.height, desc.graphics_api);
    m_window->add_event_callback_func([&](Event& event) { m_application->on_event(event); });

    create_game_context(m_window);

    // Init Renderer
    std::vector<const char*> vulkan_instance_extensions = m_window->get_vulkan_instance_extensions();

    ApiSpecificConfiguration specific_config;
    switch (desc.graphics_api)
    {
    case GraphicsApi::Dx12:
        specific_config = Dx12SpecificConfiguration{};
        break;
    case GraphicsApi::Vulkan:
        specific_config = VulkanSpecificConfiguration{
            .binding_offsets = VulkanBindingOffsets{},
            .instance_extensions = vulkan_instance_extensions,
        };
        break;
    }

    DeviceCreationDescription config{};
    config.api = desc.graphics_api;
    config.specific_config = specific_config;
    config.application_name = desc.name;
    config.application_version = desc.version;
    config.engine_name = "MizuEngine";
    config.engine_version = Version{0, 1, 0};

    g_render_device = Device::create(config);

    ShaderManager::get().add_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_PATH);

    return true;
}

void MainLoop::run()
{
    StateManagerCoordinator coordinator;
    TickInfo tick_info;
    SceneRenderer renderer{m_window};

#ifdef MIZU_MAIN_LOOP_SINGLE_THREADED
    run_single_threaded(coordinator, tick_info, renderer);
#else
    run_multi_threaded(coordinator, tick_info, renderer);
#endif
}

void MainLoop::run_single_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer)
{
    Application::instance()->on_init();

    m_shutdown_counter.store(1);

    spawn_single_threaded_job(coordinator, tick_info, *m_window, renderer);

    g_job_system->run_thread_as_worker(ThreadAffinity_Main);

    g_job_system->wait_workers_are_dead();
}

void MainLoop::spawn_single_threaded_job(
    StateManagerCoordinator& coordinator,
    TickInfo& tick_info,
    Window& window,
    SceneRenderer& renderer)
{
    MIZU_PROFILE_SCOPED;

    const Job poll_events_job =
        Job::create(&MainLoop::poll_events_job, std::ref(window)).set_affinity(ThreadAffinity_Main);
    const JobSystemHandle poll_events_handle = g_job_system->schedule(poll_events_job);

    const Job work_job = Job::create([&]() {
                             sim_job(coordinator, tick_info, window);
                             rend_job(coordinator, renderer, window);
                         })
                             .set_affinity(ThreadAffinity_Simulation)
                             .depends_on(poll_events_handle);
    const JobSystemHandle handle = g_job_system->schedule(work_job);

    if (!window.should_close())
    {
        const Job spawn_job = Job::create(
                                  &MainLoop::spawn_single_threaded_job,
                                  std::ref(coordinator),
                                  std::ref(tick_info),
                                  std::ref(window),
                                  std::ref(renderer))
                                  .depends_on(handle);
        g_job_system->schedule(spawn_job);
    }
    else
    {
        const Job shutdown_job =
            Job::create(&MainLoop::shutdown_job).set_affinity(ThreadAffinity_Main).depends_on(handle);
        g_job_system->schedule(shutdown_job);
    }
}

void MainLoop::run_multi_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer)
{
    Application::instance()->on_init();

    const uint32_t states_in_flight = BaseStateManagerConfig::MaxStatesInFlight;
    m_shutdown_counter.store(states_in_flight);

    for (uint32_t i = 0; i < states_in_flight; ++i)
        spawn_multi_threaded_jobs(coordinator, tick_info, *m_window, renderer);

    g_job_system->run_thread_as_worker(ThreadAffinity_Main);

    g_job_system->wait_workers_are_dead();
}

void MainLoop::spawn_multi_threaded_jobs(
    StateManagerCoordinator& coordinator,
    TickInfo& tick_info,
    Window& window,
    SceneRenderer& renderer)
{
    MIZU_PROFILE_SCOPED;

    const Job poll_events_job =
        Job::create(&MainLoop::poll_events_job, std::ref(window)).set_affinity(ThreadAffinity_Main);
    const JobSystemHandle poll_events_handle = g_job_system->schedule(poll_events_job);

    const Job sim_job = Job::create(&MainLoop::sim_job, std::ref(coordinator), std::ref(tick_info), std::ref(window))
                            .set_affinity(ThreadAffinity_Simulation)
                            .depends_on(poll_events_handle);
    const JobSystemHandle sim_handle = g_job_system->schedule(sim_job);

    const Job rend_job = Job::create(&MainLoop::rend_job, std::ref(coordinator), std::ref(renderer), std::ref(window))
                             .set_affinity(ThreadAffinity_Render)
                             .depends_on(sim_handle);
    const JobSystemHandle rend_handle = g_job_system->schedule(rend_job);

    if (!window.should_close())
    {
        const Job spawn_jobs = Job::create(
                                   &MainLoop::spawn_multi_threaded_jobs,
                                   std::ref(coordinator),
                                   std::ref(tick_info),
                                   std::ref(window),
                                   std::ref(renderer))
                                   .depends_on(rend_handle);
        g_job_system->schedule(spawn_jobs);
    }
    else
    {
        const Job shutdown_job =
            Job::create(&MainLoop::shutdown_job).set_affinity(ThreadAffinity_Main).depends_on(rend_handle);
        g_job_system->schedule(shutdown_job);
    }
}

void MainLoop::poll_events_job(Window& window)
{
    MIZU_PROFILE_SCOPED;

    window.poll_events();
}

void MainLoop::sim_job(StateManagerCoordinator& coordinator, TickInfo& tick_info, Window& window)
{
    MIZU_PROFILE_SCOPED;

    Application& application = *Application::instance();

    const double current_time = window.get_current_time();
    const double ts = current_time - tick_info.last_time;
    tick_info.last_time = current_time;

    coordinator.sim_begin_tick();
    {
        application.on_update(ts);
    }
    coordinator.sim_end_tick();
}

void MainLoop::rend_job(StateManagerCoordinator& coordinator, SceneRenderer& renderer, Window& window)
{
    MIZU_PROFILE_SCOPED;

    coordinator.rend_begin_frame();
    {
        renderer.render();
    }
    coordinator.rend_end_frame();

    window.swap_buffers();

    MIZU_PROFILE_FRAME_MARK;
}

void MainLoop::shutdown_job()
{
    MIZU_PROFILE_SCOPED;

    const uint32_t prev_counter = m_shutdown_counter.fetch_sub(1, std::memory_order_relaxed);
    if (prev_counter - 1 == 0)
        g_job_system->kill();
}

} // namespace Mizu