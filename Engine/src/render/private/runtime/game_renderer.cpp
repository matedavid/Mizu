#include "render/runtime/game_renderer.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/runtime.h"
#include "core/settings_manager/settings_manager.h"
#include "core/window.h"
#include "render_core/allocators/gpu_page_allocator.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"
#if MIZU_IMGUI_ENABLED
#include "imgui/imgui_panels.h"
#endif

#include "registries/light_registry.h"
#include "registries/render_settings_registry.h"
#include "registries/render_view_registry.h"
#include "registries/transform_registry.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph/render_graph_transient_memory.h"
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
#include "render/streaming/streaming_settings.h"
#include "render/systems/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render/systems/shader_manager.h"
#include "render/utils/fullscreen_helpers.h"
#include "resources/asset_load_scheduler.h"
#include "resources/material_system.h"
#include "runtime/swapchain_manager.h"
#include "scene/render_scene.h"
#include "streaming/mesh_streaming_manager.h"
#include "streaming/sparse_bind_scheduler.h"
#include "streaming/staging_pool.h"
#include "streaming/streaming_planner.h"
#include "streaming/texture_streaming_manager.h"

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

    if (!init_modules())
    {
        MIZU_LOG_ERROR("Failed to initialize render modules");
        return false;
    }

#if MIZU_IMGUI_ENABLED
    add_imgui_debug_panels();
#endif

    return true;
}

GameRenderer::~GameRenderer()
{
    if (g_render_device != nullptr)
    {
        g_render_device->wait_idle();
    }

    shutdown_modules();

    shutdown_renderer();
    shutdown_asset_systems();
    shutdown_registries();
    shutdown_state_managers();
    shutdown_render_device();
}

bool GameRenderer::acquire_swapchain_image()
{
    MIZU_PROFILE_SCOPED;

    for (const SyncPoint& point : m_frame_completion[m_frame_in_flight_idx])
    {
        point.wait();
    }

    return m_swapchain_manager->acquire_next_image();
}

void GameRenderer::set_frame_timing(const RenderFrameTiming& frame_timing)
{
    m_frame_timings[m_frame_in_flight_idx] = frame_timing;

    constexpr double FRAME_DELTA_SMOOTHING = 0.05;

    const double average = m_average_frame_delta_seconds.load(std::memory_order_relaxed);
    const double new_average = average == 0.0
                                   ? frame_timing.frame_delta_seconds
                                   : average + (frame_timing.frame_delta_seconds - average) * FRAME_DELTA_SMOOTHING;
    m_average_frame_delta_seconds.store(new_average, std::memory_order_relaxed);
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

    m_render_scene->update();
    m_material_system->update(m_frame_in_flight_idx);

    const uint32_t width = m_swapchain_manager->get_current_image()->get_width();
    const uint32_t height = m_swapchain_manager->get_current_image()->get_height();
    m_streaming_planner->update({
        .frame_num = m_current_frame,
        .views = render_view_registry_get_views(),
        .viewport_size = {width, height},
    });

    m_asset_load_scheduler->dispatch_load_jobs();
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

    for (uint32_t label_idx = 0; label_idx < m_render_modules.size(); ++label_idx)
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
        .material_system = *m_material_system,
    });

    RenderModuleFrameData render_module_frame_data{
        .output_texture = swapchain_texture,
    };

    m_streaming_planner->add_gpu_uploads_pass(builder, *m_frame_linear_allocator);
    transform_registry_add_transform_publish_pass(builder, *m_frame_linear_allocator);

    draw_list_system_add_passes(builder, *m_frame_linear_allocator);

    for (uint32_t label_idx = 0; label_idx < m_render_modules.size(); ++label_idx)
    {
        IRenderModule* module = m_render_modules[label_idx];
        if (module == nullptr)
            continue;

        render_module_frame_data.label = static_cast<RenderModuleLabel>(label_idx);

        module->build_render_graph(builder, blackboard, render_module_frame_data);
    }

    draw_list_system_finalize(builder, *m_frame_linear_allocator);
}

void GameRenderer::compile_render_graph_job()
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphBuilderCompileOptions builder_compile_options{
        *m_render_graph_transient_memory, *m_render_graph_resource_registry};

    RenderGraph& render_graph = m_render_graphs[m_frame_in_flight_idx];
    m_render_graph_builder.compile(render_graph, builder_compile_options);
}

void GameRenderer::prepare_draw_lists_job()
{
    MIZU_PROFILE_SCOPED;

    draw_list_system_prepare(*m_frame_linear_allocator);
}

void GameRenderer::execute_render_graph_job()
{
    MIZU_PROFILE_SCOPED;

    SubmitInfo submit_info{};
    submit_info.waits.push_back(m_swapchain_manager->get_image_ready());

    const SyncPoint sparse_binds_sync = m_sparse_bind_scheduler->flush({});
    if (sparse_binds_sync.is_valid())
    {
        submit_info.waits.push_back(sparse_binds_sync);
    }

    RenderGraph& render_graph = m_render_graphs[m_frame_in_flight_idx];
    m_rendering_done = render_graph.execute(submit_info);

    meta::enum_array<CommandBufferType, SyncPoint>& frame_completion = m_frame_completion[m_frame_in_flight_idx];
    frame_completion[CommandBufferType::Graphics] =
        g_render_device->get_queue(CommandBufferType::Graphics).get_last_submitted();
    frame_completion[CommandBufferType::Compute] =
        g_render_device->get_queue(CommandBufferType::Compute).get_last_submitted();
    frame_completion[CommandBufferType::Transfer] =
        g_render_device->get_queue(CommandBufferType::Transfer).get_last_submitted();
}

void GameRenderer::present_job()
{
    MIZU_PROFILE_SCOPED;

    m_swapchain_manager->present(m_rendering_done);

    m_frame_in_flight_idx = (m_frame_in_flight_idx + 1) % m_frames_in_flight;
    m_current_frame += 1;

    MIZU_PROFILE_FRAME_MARK;
}

bool GameRenderer::init_modules()
{
    for (IRenderModule* module : m_render_modules)
    {
        if (module == nullptr)
            continue;

        if (!module->init())
            return false;
    }

    return true;
}

void GameRenderer::shutdown_modules()
{
    for (IRenderModule*& module : m_render_modules | std::views::reverse)
    {
        if (module == nullptr)
            continue;

        module->shutdown();

        delete module;
        module = nullptr;
    }
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
    MIZU_LOG_INFO("    GpuDrivenRendering: {}", settings.gpu_driven_rendering_enabled);

    const DeviceProperties& device_props = g_render_device->get_properties();
    MIZU_LOG_INFO("Created Device on {}", device_props.name);
    MIZU_LOG_INFO("    DepthClampEnabled:  {}", device_props.depth_clamp);
    MIZU_LOG_INFO("    AsyncCompute:       {}", device_props.async_compute);
    MIZU_LOG_INFO("    AsyncTransfer:      {}", device_props.async_transfer);
    MIZU_LOG_INFO("    RayTracingHardware: {}", device_props.ray_tracing.supported);
    MIZU_LOG_INFO("    SparseBuffers:      {}", device_props.sparse.buffers);
    MIZU_LOG_INFO("    SparseImages:       {}", device_props.sparse.images);
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
    swapchain_manager_desc.present_mode = PresentMode::Mailbox;

    m_swapchain_manager = std::make_unique<SwapchainManager>();
    if (!m_swapchain_manager->init(swapchain_manager_desc))
    {
        MIZU_LOG_ERROR("Failed to initialize SwapchainManager");
        return false;
    }

    m_frame_completion.resize(m_frames_in_flight);
    m_frame_timings.resize(m_frames_in_flight);
    m_render_graphs.resize(m_frames_in_flight);

    m_frame_in_flight_idx = 0;

    m_render_graph_transient_memory =
        std::make_unique<RenderGraphTransientMemory>("GameRenderer::RenderGraphTransientMemory");
    m_render_graph_resource_registry = std::make_unique<RenderGraphResourceRegistry>();

    constexpr uint64_t FRAME_LINEAR_ALLOCATOR_PER_FRAME_SIZE = 256ull * 1024 * 1024; // 256 MiB
    m_frame_linear_allocator = std::make_unique<FrameLinearAllocator>(
        m_frames_in_flight, FRAME_LINEAR_ALLOCATOR_PER_FRAME_SIZE, "GameRenderer::FrameLinearAllocator");

    m_render_scene = std::make_unique<RenderScene>(*m_streaming_planner, *m_material_system);

    const MeshStreamingManager* mesh_streaming_manager =
        m_streaming_planner->get_streaming_manager<MeshStreamingManager>();
    MIZU_VERIFY(mesh_streaming_manager != nullptr, "GameRenderer requires a MeshStreamingManager");

    draw_list_system_init(*m_render_scene, mesh_streaming_manager->get_streaming_pool());

    const bool fullscreen_helpers_ok = FullscreenHelpers::init();

    // clang-format off
    return m_render_graph_transient_memory      != nullptr 
        && m_render_graph_resource_registry     != nullptr
        && m_frame_linear_allocator             != nullptr
        && fullscreen_helpers_ok;
    // clang-format on
}

void GameRenderer::shutdown_renderer()
{
    FullscreenHelpers::shutdown();

    draw_list_system_shutdown();

    m_render_scene.reset();

    m_render_graph_builder.reset();

    for (size_t i = 0; i < m_frames_in_flight; ++i)
    {
        m_render_graphs[i].reset();
    }

    m_frame_completion.clear();
    m_rendering_done = SyncPoint{};

    m_frame_linear_allocator.reset();
    m_render_graph_resource_registry.reset();
    m_render_graph_transient_memory.reset();

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
    transform_registry_init();
    light_registry_init();
    render_view_registry_init();
    render_settings_registry_init();

    return true;
}

void GameRenderer::shutdown_registries()
{
    transform_registry_shutdown();
    light_registry_shutdown();
    render_view_registry_shutdown();
    render_settings_registry_shutdown();
}

bool GameRenderer::init_asset_systems()
{
    // TODO: Memory type should be compatible with the sparse textures memory_type_bits
    const std::optional<MemoryTypeId> page_memory_type =
        g_render_device->find_memory_type(MemoryRequirements{.memory_type_bits = ~0u}, HeapDomain::DeviceLocal);
    if (!page_memory_type.has_value())
    {
        MIZU_LOG_ERROR("Failed to find memory type for GpuPageAllocator");
        return false;
    }

    m_gpu_page_allocator = std::make_unique<GpuPageAllocator>(
        *g_render_device,
        GpuPageAllocatorDescription{
            .domain = HeapDomain::DeviceLocal,
            .memory_type = *page_memory_type,
            .name = "GameRenderer::GpuPageAllocator",
        });

    m_sparse_bind_scheduler = std::make_unique<SparseBindScheduler>();

    constexpr uint64_t STAGING_POOL_BUDGET = 128ull * 1024 * 1024;
    m_staging_pool = std::make_unique<StagingPool>(STAGING_POOL_BUDGET);

    IAssetLoader& asset_loader = g_game_context->get_asset_loader();

    m_asset_load_scheduler = std::make_unique<AssetLoadScheduler>(asset_loader);

    m_streaming_planner = std::make_unique<StreamingPlanner>(*m_staging_pool);

    constexpr uint64_t MESH_POOL_VERTEX_BUFFER_SIZE = 512ull * 1024 * 1024;
    constexpr uint64_t MESH_POOL_INDEX_BUFFER_SIZE = 256ull * 1024 * 1024;

    const MeshStreamingManagerDescription mesh_streaming_desc{
        .pool =
            MeshStreamingPoolDescription{
                .vertex_buffer_size = MESH_POOL_VERTEX_BUFFER_SIZE,
                .index_buffer_size = MESH_POOL_INDEX_BUFFER_SIZE,
                .release_delay_frames = m_frames_in_flight + 2,
            },
    };
    m_streaming_planner->create_streaming_manager<MeshStreamingManager>(
        mesh_streaming_desc,
        asset_loader,
        *m_asset_load_scheduler,
        *m_staging_pool,
        *m_gpu_page_allocator,
        *m_sparse_bind_scheduler);

    const TextureStreamingManagerDescription texture_streaming_desc{
        .pool =
            TextureStreamingPoolDescription{
                .release_delay_frames = m_frames_in_flight + 2,
            },
    };
    TextureStreamingManager& texture_streaming_manager =
        m_streaming_planner->create_streaming_manager<TextureStreamingManager>(
            texture_streaming_desc,
            asset_loader,
            *m_asset_load_scheduler,
            *m_staging_pool,
            *m_gpu_page_allocator,
            *m_sparse_bind_scheduler);

    m_material_system = std::make_unique<MaterialSystem>(texture_streaming_manager, asset_loader, m_frames_in_flight);

    return true;
}

void GameRenderer::shutdown_asset_systems()
{
    m_material_system.reset();

    m_streaming_planner.reset();

    m_asset_load_scheduler.reset();

    m_staging_pool.reset();

    m_sparse_bind_scheduler.reset();
    m_gpu_page_allocator.reset();
}

#if MIZU_IMGUI_ENABLED

void GameRenderer::add_imgui_debug_panels()
{
    ImGuiPanels::register_panel(ImGuiPanelBuilder::window("GameRenderer").menu("Render").closed(), [this]() {
        ImGui::SeparatorText("Stats");
        {
            const double frame_delta = m_average_frame_delta_seconds.load(std::memory_order_relaxed);
            if (frame_delta > 0.0)
                ImGui::Text("Render Cpu: %.1f fps (%.2f ms)", 1.0 / frame_delta, frame_delta * 1000.0);
        }

        ImGui::SeparatorText("RendererSettings");
        {
            RendererSettings& settings = get_setting<RendererSettings>();
            ImGui::Checkbox("GpuDrivenRenderingEnabled", &settings.gpu_driven_rendering_enabled);
        }

        ImGui::SeparatorText("DeviceProperties");
        {
            const DeviceProperties& props = g_render_device->get_properties();

            ImGui::Text("Name: %s", props.name.c_str());
            ImGui::Text("Depth clamp: %s", props.depth_clamp ? "true" : "false");
            ImGui::Text("Async compute: %s", props.async_compute ? "true" : "false");
            ImGui::Text("Async transfer: %s", props.async_transfer ? "true" : "false");
            ImGui::Text("Max bindless descriptors: %u", props.max_bindless_descriptors);

            ImGui::TextUnformatted("Alignments");
            ImGui::Indent();
            ImGui::Text(
                "Min constant buffer offset: %llu",
                static_cast<unsigned long long>(props.alignments.min_constant_buffer_offset));
            ImGui::Text(
                "Min raw buffer offset: %llu", static_cast<unsigned long long>(props.alignments.min_raw_buffer_offset));
            ImGui::Text(
                "Min texture row pitch: %llu", static_cast<unsigned long long>(props.alignments.min_texture_row_pitch));
            ImGui::Text(
                "Min texture data placement: %llu",
                static_cast<unsigned long long>(props.alignments.min_texture_data_placement));
            ImGui::Unindent();

            ImGui::TextUnformatted("Ray tracing");
            ImGui::Indent();
            ImGui::Text("Supported: %s", props.ray_tracing.supported ? "true" : "false");
            ImGui::Text(
                "Acceleration structure instance size: %llu bytes",
                static_cast<unsigned long long>(props.ray_tracing.acceleration_structure_instance_size));
            ImGui::Unindent();

            ImGui::TextUnformatted("Sparse resources");
            ImGui::Indent();
            ImGui::Text("Buffers: %s", props.sparse.buffers ? "true" : "false");
            ImGui::Text("Images: %s", props.sparse.images ? "true" : "false");
            ImGui::Text("Non resident strict: %s", props.sparse.non_resident_strict ? "true" : "false");
            ImGui::Text(
                "Address space size: %.2f GB",
                static_cast<double>(props.sparse.address_space_size) / (1024.0 * 1024.0 * 1024.0));
            ImGui::Unindent();
        }
    });

    ImGuiPanels::register_panel(ImGuiPanelBuilder::window("Streaming").menu("Render").closed(), [this]() {
        ImGui::SeparatorText("Settings");

        StreamingSettings& settings = get_setting<StreamingSettings>();

        const char* const streaming_mode_names[] = {"Enabled", "FullResidency", "Frozen"};
        int mode = static_cast<int>(settings.mode);
        if (ImGui::Combo("Mode", &mode, streaming_mode_names, static_cast<int>(std::size(streaming_mode_names))))
            settings.mode = static_cast<StreamingMode>(mode);

        ImGui::Checkbox("LoggingEnabled", &settings.logging_enabled);

        ImGui::SeparatorText("Asset loading");
        ImGui::Text("Load jobs in flight: %u", m_asset_load_scheduler->get_load_jobs_in_flight());

        const TextureStreamingManager* texture_manager =
            m_streaming_planner->get_streaming_manager<TextureStreamingManager>();
        if (texture_manager != nullptr)
        {
            ImGui::SeparatorText("Textures");
            draw_imgui_texture_streaming_stats(texture_manager->get_streaming_pool().get_statistics());
        }

        const MeshStreamingManager* mesh_manager = m_streaming_planner->get_streaming_manager<MeshStreamingManager>();
        if (mesh_manager != nullptr)
        {
            const MeshStreamingPool& mesh_pool = mesh_manager->get_streaming_pool();

            ImGui::SeparatorText("Meshes");
            draw_imgui_mesh_streaming_stats(
                mesh_pool.get_backing(), mesh_pool.get_vertex_statistics(), mesh_pool.get_index_statistics());
        }
    });
}

#endif

void setup_default_game_renderer(GameRenderer& renderer)
{
    renderer.register_module<SceneRenderer>(RenderModuleLabel::Scene);
}

} // namespace Mizu
