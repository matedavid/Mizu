#include "render/runtime/game_renderer.h"

#include "asset/dev_asset_loader.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/runtime.h"
#include "core/settings_manager/settings_manager.h"
#include "core/window.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"

#include "registries/light_registry.h"
#include "registries/render_settings_registry.h"
#include "registries/render_view_registry.h"
#include "registries/renderable_registry.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/runtime/renderer.h"
#include "render/runtime/renderer_settings.h"
#include "render/scene/draw_list_system.h"
#include "render/scene/scene_renderer.h"
#include "render/state_manager/light_state_manager.h"
#include "render/state_manager/render_settings_layer_state_manager.h"
#include "render/state_manager/render_settings_volume_state_manager.h"
#include "render/state_manager/render_view_state_manager.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"
#include "render/systems/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render/systems/shader_manager.h"
#include "render/utils/fullscreen_helpers.h"
#include "resources/asset_load_system.h"
#include "resources/cpu_loading_pool.h"
#include "resources/gpu_pools.h"
#include "resources/residency_system.h"
#include "resources/resource_event_stream.h"
#include "resources/streaming_planner.h"
#include "runtime/swapchain_manager.h"
#include "scene/scene_system.h"

namespace Mizu
{

MIZU_REGISTER_SETTING(RendererSettings);

GameRenderer::GameRenderer()
{
    for (size_t i = 0; i < m_render_modules.size(); ++i)
    {
        m_render_modules[i] = nullptr;
    }
}

bool GameRenderer::init(const GameRendererDescription& desc)
{
    if (g_render_device != nullptr)
    {
        MIZU_LOG_ERROR("GameRenderer already initialized");
        return false;
    }

    m_window = desc.window;

    const RendererSettings& settings = get_setting<RendererSettings>();

    constexpr uint32_t MIN_FRAMES_IN_FLIGHT = 1;
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    m_frames_in_flight = std::clamp(settings.frames_in_flight, MIN_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);

    if (m_frames_in_flight != settings.frames_in_flight)
    {
        MIZU_LOG_WARNING(
            "Clamped requested frames in flight ({}) to the allowed range of [{} {}]",
            settings.frames_in_flight,
            MIN_FRAMES_IN_FLIGHT,
            MAX_FRAMES_IN_FLIGHT);
    }

    if (!init_render_device(desc))
    {
        MIZU_LOG_ERROR("Failed to initialize render device");
        return false;
    }

    if (!init_state_managers())
    {
        MIZU_LOG_ERROR("Failed to initialize state managers");
        return false;
    }

    if (!init_registries())
    {
        MIZU_LOG_ERROR("Failed to initialize registries");
        return false;
    }

    if (!init_asset_systems())
    {
        MIZU_LOG_ERROR("Failed to initialize asset systems");
        return false;
    }

    if (!init_renderer())
    {
        MIZU_LOG_ERROR("Failed to initialize renderer");
        return false;
    }

    return true;
}

GameRenderer::~GameRenderer()
{
    if (g_render_device != nullptr)
    {
        g_render_device->wait_idle();
    }

    for (IRenderModule* module : m_render_modules)
    {
        delete module;
    }

    shutdown_renderer();
    shutdown_asset_systems();
    shutdown_registries();
    shutdown_state_managers();
    shutdown_render_device();
}

void GameRenderer::acquire_swapchain_image()
{
    m_swapchain_manager->acquire_next_image(m_frame_in_flight_idx, m_fences[m_frame_in_flight_idx]);
}

void GameRenderer::set_frame_timing(const RenderFrameTiming& frame_timing)
{
    m_frame_timings[m_frame_in_flight_idx] = frame_timing;
}

JobHandle GameRenderer::create_update_jobs(const JobHandle& wait_job)
{
    const JobHandle prepare_frame_update_systems_batch =
        g_job_system->schedule_batch()
            .add(JobDescription::create(&GameRenderer::prepare_frame_job, this).name("PrepareFrame"))
            .add(JobDescription::create(&GameRenderer::update_systems_job, this).name("UpdateSystems"))
            .depends_on(wait_job)
            .submit();

    inplace_vector<JobHandle, RENDER_MODULE_LABEL_COUNT> update_job_handles{};
    get_render_module_update_job_handles(prepare_frame_update_systems_batch, update_job_handles);

    const JobHandle build_render_graph_job = g_job_system->schedule(&GameRenderer::build_render_graph_job, this)
                                                 .depends_on(prepare_frame_update_systems_batch)
                                                 .depends_on(update_job_handles)
                                                 .name("BuildRenderGraph")
                                                 .submit();

    const JobHandle compile_render_graph_prepare_draw_lists_batch =
        g_job_system->schedule_batch()
            .add(JobDescription::create(&GameRenderer::compile_render_graph_job, this).name("CompileRenderGraph"))
            .add(JobDescription::create(&GameRenderer::prepare_draw_lists_job, this).name("PrepareDrawLists"))
            .depends_on(build_render_graph_job)
            .submit();

    const JobHandle execute_render_graph_job = g_job_system->schedule(&GameRenderer::execute_render_graph_job, this)
                                                   .depends_on(compile_render_graph_prepare_draw_lists_batch)
                                                   .name("ExecuteRenderGraph")
                                                   .submit();

    const JobHandle present_job = g_job_system->schedule(&GameRenderer::present_job, this)
                                      .depends_on(execute_render_graph_job)
                                      .name("Present")
                                      .submit();

    return present_job;
}

void GameRenderer::prepare_frame_job()
{
    MIZU_PROFILE_SCOPED;

    g_render_device->prepare_frame(m_frame_in_flight_idx);
    m_frame_linear_allocator->prepare_frame(m_frame_in_flight_idx);

    m_render_graph_builder.reset();

    draw_list_system_reset();
}

void GameRenderer::update_systems_job()
{
    MIZU_PROFILE_SCOPED;

    render_settings_registry_update();

    light_registry_update();

    ResourceEventStream& event_stream = *m_resource_event_stream;
    event_stream.reset();

    renderable_registry_update(event_stream);

    m_streaming_planner->update(event_stream);

    // TODO: Could be parallelized into multiple jobs
    m_mesh_residency_system->update(event_stream, m_current_frame);
    m_texture_residency_system->update(event_stream, m_current_frame);
    m_material_residency_system->update(event_stream, m_current_frame);

    m_scene_system->update(event_stream, m_current_frame);
    m_asset_load_system->dispatch_load_jobs();
}

void GameRenderer::get_render_module_update_job_handles(
    const JobHandle& wait_job,
    inplace_vector<JobHandle, RENDER_MODULE_LABEL_COUNT>& out_update_jobs)
{
    RenderModuleUpdateContext update_context{
        .frame_num = m_current_frame,
        .frame_in_flight_idx = m_frame_in_flight_idx,
        .last_frame_seconds = m_frame_timings[m_frame_in_flight_idx].frame_delta_seconds,
        .frame_allocator = *m_frame_linear_allocator,
        .wait_job = wait_job,
    };

    for (uint32_t label_idx = 0; label_idx < RENDER_MODULE_LABEL_COUNT; ++label_idx)
    {
        IRenderModule* module = m_render_modules[label_idx];
        if (module == nullptr)
            continue;

        update_context.label = static_cast<RenderModuleLabel>(label_idx);

        const JobHandle job = module->create_update_jobs(update_context);

        if (job != wait_job)
        {
            out_update_jobs.push_back(job);
        }
    }
}

void GameRenderer::build_render_graph_job()
{
    MIZU_PROFILE_SCOPED;

    const auto swapchain_image = m_swapchain_manager->get_current_image();
    const RenderFrameTiming& frame_timing = m_frame_timings[m_frame_in_flight_idx];

    RenderGraphBuilder& builder = m_render_graph_builder;
    RenderGraphBlackboard blackboard{};

    const RenderGraphResource swapchain_texture = builder.register_external_texture(
        swapchain_image, {.initial_state = ImageResourceState::Undefined, .final_state = ImageResourceState::Present});

    FrameData& frame_data = blackboard.add<FrameData>();
    frame_data.frame_num = m_current_frame;
    frame_data.frame_in_flight_idx = m_frame_in_flight_idx;
    frame_data.last_frame_seconds = frame_timing.frame_delta_seconds;

    blackboard.add<RenderSystemsData>({
        .frame_allocator = *m_frame_linear_allocator,
        .texture_residency_system = *m_texture_residency_system,
        .material_residency_system = *m_material_residency_system,
    });

    RenderModuleFrameData render_module_frame_data{
        .output_texture = swapchain_texture,
    };

    m_asset_load_system->add_gpu_uploads_pass(builder);
    m_scene_system->add_transform_publish_pass(builder, *m_frame_linear_allocator);

    for (uint32_t label_idx = 0; label_idx < RENDER_MODULE_LABEL_COUNT; ++label_idx)
    {
        IRenderModule* module = m_render_modules[label_idx];
        if (module == nullptr)
            continue;

        render_module_frame_data.label = static_cast<RenderModuleLabel>(label_idx);

        module->build_render_graph(builder, blackboard, render_module_frame_data);
    }
}

void GameRenderer::compile_render_graph_job()
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphBuilderCompileOptions builder_compile_options{
        *m_render_graph_transient_memory_pool, *m_render_graph_resource_registry};

    RenderGraph& render_graph = m_render_graphs[m_frame_in_flight_idx];
    m_render_graph_builder.compile(render_graph, builder_compile_options);
}

void GameRenderer::prepare_draw_lists_job()
{
    MIZU_PROFILE_SCOPED;

    draw_list_system_compile_draw_lists();
    draw_list_system_build_frame_resources(*m_frame_linear_allocator);
}

void GameRenderer::execute_render_graph_job()
{
    MIZU_PROFILE_SCOPED;

    const auto& image_acquired_semaphore = m_swapchain_manager->get_image_acquired_semaphore();
    const auto& render_finished_semaphore = m_swapchain_manager->get_render_finished_semaphore();

    CommandBufferSubmitInfo submit_info{};
    submit_info.wait_semaphores = {image_acquired_semaphore};
    submit_info.signal_semaphores = {render_finished_semaphore};
    submit_info.signal_fence = m_fences[m_frame_in_flight_idx];

    RenderGraph& render_graph = m_render_graphs[m_frame_in_flight_idx];
    render_graph.execute(submit_info);
}

void GameRenderer::present_job()
{
    MIZU_PROFILE_SCOPED;

    m_swapchain_manager->present();

    m_frame_in_flight_idx = (m_frame_in_flight_idx + 1) % m_frames_in_flight;
    m_current_frame += 1;

    MIZU_PROFILE_FRAME_MARK;
}

bool GameRenderer::init_render_device(const GameRendererDescription& desc)
{
    const RendererSettings& settings = get_setting<RendererSettings>();

    std::vector<const char*> vulkan_instance_extensions = m_window->get_vulkan_instance_extensions();

    ApiSpecificConfiguration specific_config;
    switch (settings.graphics_api)
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
    config.api = settings.graphics_api;
    config.specific_config = specific_config;
    config.frames_in_flight = m_frames_in_flight;
    config.validations_enabled = settings.validations_enabled;
    config.application_name = desc.application_name;
    config.application_version = desc.application_version;
    config.engine_name = "MizuEngine";
    config.engine_version = Version{0, 1, 0};

    g_render_device = Device::create(config);
    if (g_render_device == nullptr)
        return false;

#if MIZU_LOGGING_ENABLED
    MIZU_LOG_INFO("Initializing GameRenderer:");
    MIZU_LOG_INFO("    GraphicsApi:        {}", meta::enum_name(settings.graphics_api));
    MIZU_LOG_INFO("    ValidationsEnabled: {}", settings.validations_enabled);
    MIZU_LOG_INFO("    FramesInFlight:     {}", m_frames_in_flight);

    const DeviceProperties& device_props = g_render_device->get_properties();
    MIZU_LOG_INFO("Created Device on {}", device_props.name);
    MIZU_LOG_INFO("    DepthClampEnabled:  {}", device_props.depth_clamp_enabled);
    MIZU_LOG_INFO("    AsyncCompute:       {}", device_props.async_compute);
    MIZU_LOG_INFO("    AsyncTransfer:      {}", device_props.async_transfer);
    MIZU_LOG_INFO("    RayTracingHardware: {}", device_props.ray_tracing_hardware);
#endif

    return true;
}

void GameRenderer::shutdown_render_device()
{
    delete g_render_device;
    g_render_device = nullptr;

    Device::free();
}

bool GameRenderer::init_renderer()
{
    SwapchainManagerDescription swapchain_manager_desc{};
    swapchain_manager_desc.window = m_window;
    // TODO: Revisit this format, done because Dx12 DXGI_SWAP_EFFECT_FLIP_DISCARD does not support SRGB formats.
    swapchain_manager_desc.format = ImageFormat::R8G8B8A8_UNORM;
    swapchain_manager_desc.frames_in_flight = m_frames_in_flight;
    swapchain_manager_desc.present_mode = PresentMode::Mailbox;

    m_swapchain_manager = std::make_unique<SwapchainManager>();
    if (!m_swapchain_manager->init(swapchain_manager_desc))
    {
        MIZU_LOG_ERROR("Failed to initialize SwapchainManager");
        return false;
    }

    m_fences.resize(m_frames_in_flight);
    m_frame_timings.resize(m_frames_in_flight);
    m_render_graphs.resize(m_frames_in_flight);

    m_frame_in_flight_idx = 0;
    for (size_t i = 0; i < m_frames_in_flight; ++i)
    {
        m_fences[i] = g_render_device->create_fence();
    }

    m_render_graph_transient_memory_pool =
        g_render_device->create_transient_memory_pool("GameRenderer_TransientMemoryPool");
    m_render_graph_resource_registry = std::make_unique<RenderGraphResourceRegistry>();

    constexpr uint64_t FRAME_LINEAR_ALLOCATOR_PER_FRAME_SIZE = 256ull * 1024 * 1024; // 256 MiB
    m_frame_linear_allocator = std::make_unique<FrameLinearAllocator>(
        m_frames_in_flight, FRAME_LINEAR_ALLOCATOR_PER_FRAME_SIZE, "GameRenderer_FrameLinearAllocator");

    m_scene_system = std::make_unique<SceneSystem>(*m_mesh_residency_system, *m_material_residency_system);

    draw_list_system_init(*m_scene_system, *m_gpu_mesh_pool);

    ShaderManager::get().add_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_PATH);

    const bool fullscreen_helpers_ok = FullscreenHelpers::init();

    // clang-format off
    return m_render_graph_transient_memory_pool != nullptr 
        && m_render_graph_resource_registry     != nullptr
        && m_frame_linear_allocator             != nullptr
        && fullscreen_helpers_ok;
    // clang-format on
}

void GameRenderer::shutdown_renderer()
{
    FullscreenHelpers::shutdown();

    draw_list_system_shutdown();

    m_scene_system.reset();

    m_render_graph_builder.reset();

    for (size_t i = 0; i < m_frames_in_flight; ++i)
    {
        m_render_graphs[i].reset();

        m_fences[i].reset();
    }

    m_frame_linear_allocator.reset();
    m_render_graph_resource_registry.reset();
    m_render_graph_transient_memory_pool.reset();

    m_swapchain_manager.reset();

    ShaderManager::get().reset();
    PipelineCache::get().reset();
    SamplerStateCache::get().reset();
}

bool GameRenderer::init_state_managers()
{
    MIZU_ASSERT(g_state_manager_coordinator != nullptr, "StateManagerCoordinator must be initialized");

    g_transform_state_manager = new TransformStateManager{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_transform_state_manager));

    g_render_view_state_manager = new RenderViewStateManager{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_render_view_state_manager));

    g_render_settings_layer_state_manager = new RenderSettingsLayerStateManager{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_render_settings_layer_state_manager)
            .depends_on(g_render_view_state_manager));

    g_render_settings_volume_state_manager = new RenderSettingsVolumeStateManager{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_render_settings_volume_state_manager)
            .depends_on(g_transform_state_manager)
            .depends_on(g_render_view_state_manager));

    g_static_mesh_state_manager = new StaticMeshStateManager{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_static_mesh_state_manager).depends_on(g_transform_state_manager));

    g_light_state_manager = new LightStateManager{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_light_state_manager)
            .depends_on(g_transform_state_manager)
            .depends_on(g_render_view_state_manager)
            .depends_on(g_render_settings_layer_state_manager)
            .depends_on(g_render_settings_volume_state_manager));

    return true;
}

void GameRenderer::shutdown_state_managers()
{
    delete g_render_view_state_manager;
    delete g_light_state_manager;
    delete g_static_mesh_state_manager;
    delete g_transform_state_manager;
    delete g_render_settings_layer_state_manager;
    delete g_render_settings_volume_state_manager;
}

bool GameRenderer::init_registries()
{
    renderable_registry_init();
    light_registry_init();
    render_view_registry_init();
    render_settings_registry_init();

    return true;
}

void GameRenderer::shutdown_registries()
{
    renderable_registry_shutdown();
    light_registry_shutdown();
    render_view_registry_shutdown();
    render_settings_registry_shutdown();
}

bool GameRenderer::init_asset_systems()
{
    const StreamingPlannerConfig streaming_planner_config{};
    m_streaming_planner = std::make_unique<StreamingPlanner>(streaming_planner_config);

    m_resource_event_stream = std::make_unique<ResourceEventStream>();

    static constexpr uint64_t CPU_LOADING_POOL_MESH_BUDGET = 256ull * 1024 * 1024;
    static constexpr uint64_t CPU_LOADING_POOL_TEXTURE_BUDGET = 256ull * 1024 * 1024;

    m_cpu_loading_pool = std::make_unique<CpuLoadingPool>();
    if (!m_cpu_loading_pool->init(CPU_LOADING_POOL_MESH_BUDGET, CPU_LOADING_POOL_TEXTURE_BUDGET))
    {
        MIZU_LOG_ERROR("Failed to initialize CpuLoadingPool");
        return false;
    }

    static constexpr uint64_t GPU_MESH_POOL_BUDGET = 512ull * 1024 * 1024;
    static constexpr uint64_t GPU_TEXTURE_POOL_BUDGET = 512ull * 1024 * 1024;

    m_gpu_mesh_pool = std::make_unique<GpuMeshPool>();
    m_gpu_texture_pool = std::make_unique<GpuTexturePool>();

    if (!m_gpu_mesh_pool->init(GPU_MESH_POOL_BUDGET))
    {
        MIZU_LOG_ERROR("Failed to initialize GpuMeshPool");
        return false;
    }

    if (!m_gpu_texture_pool->init(GPU_TEXTURE_POOL_BUDGET))
    {
        MIZU_LOG_ERROR("Failed to initialize GpuTexturePool");
        return false;
    }

    m_asset_loader = std::make_unique<DevAssetLoader>(g_game_context->get_asset_registry());
    m_asset_load_system =
        std::make_unique<AssetLoadSystem>(*m_asset_loader, *m_cpu_loading_pool, *m_gpu_mesh_pool, *m_gpu_texture_pool);

    m_mesh_residency_system = std::make_unique<MeshResidencySystem>(
        *m_asset_load_system, m_streaming_planner->get_mesh_request_queue(), *m_gpu_mesh_pool);
    m_texture_residency_system = std::make_unique<TextureResidencySystem>(
        *m_asset_load_system, m_streaming_planner->get_texture_request_queue(), *m_gpu_texture_pool);
    m_material_residency_system = std::make_unique<MaterialResidencySystem>(
        *m_asset_load_system, m_streaming_planner->get_material_request_queue(), *m_texture_residency_system);

    return true;
}

void GameRenderer::shutdown_asset_systems()
{
    m_material_residency_system.reset();
    m_texture_residency_system.reset();
    m_mesh_residency_system.reset();

    m_asset_load_system.reset();
    m_asset_loader.reset();

    m_gpu_texture_pool.reset();
    m_gpu_mesh_pool.reset();
    m_cpu_loading_pool.reset();

    m_resource_event_stream.reset();
    m_streaming_planner.reset();
}

void setup_default_game_renderer(GameRenderer& renderer)
{
    renderer.register_module<SceneRenderer>(RenderModuleLabel::Scene);
}

} // namespace Mizu
