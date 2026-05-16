#include "render/runtime/game_renderer.h"

#include "base/debug/profiling.h"
#include "core/runtime.h"
#include "core/window.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"

#include "light_manager.h"
#include "mesh_manager.h"
#include "render/frame_linear_allocator.h"
#include "render/passes/pass_info.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph_renderer.h"
#include "render/runtime/renderer.h"
#include "render/state_manager/camera_state_manager.h"
#include "render/state_manager/renderer_settings_state_manager.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render/systems/shader_manager.h"

namespace Mizu
{

GameRenderer::GameRenderer(const GameRendererDescription& desc) : m_window(desc.window)
{
    for (size_t i = 0; i < m_render_modules.size(); ++i)
    {
        m_render_modules[i] = nullptr;
    }

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
    config.frames_in_flight = FRAMES_IN_FLIGHT;
    config.application_name = desc.application_name;
    config.application_version = desc.application_version;
    config.engine_name = "MizuEngine";
    config.engine_version = Version{0, 1, 0};

    g_render_device = Device::create(config);

#if MIZU_DEBUG
    const DeviceProperties& device_props = g_render_device->get_properties();
    MIZU_LOG_INFO("Created Device on {}", device_props.name);
    MIZU_LOG_INFO("    DepthClampEnabled:  {}", device_props.depth_clamp_enabled);
    MIZU_LOG_INFO("    AsyncCompute:       {}", device_props.async_compute);
    MIZU_LOG_INFO("    RayTracingHardware: {}", device_props.ray_tracing_hardware);
#endif

    ShaderManager::get().add_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_PATH);

    SwapchainDescription swapchain_desc{};
    swapchain_desc.window = m_window;
    // TODO: Revisit this format, done because Dx12 DXGI_SWAP_EFFECT_FLIP_DISCARD does not support SRGB formats.
    swapchain_desc.format = ImageFormat::R8G8B8A8_UNORM;

    m_swapchain = g_render_device->create_swapchain(swapchain_desc);

    m_current_frame = 0;
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_fences[i] = g_render_device->create_fence();
        m_image_acquired_semaphores[i] = g_render_device->create_semaphore();
        m_render_finished_semaphores[i] = g_render_device->create_semaphore();
    }

    m_render_graph_transient_memory_pool =
        g_render_device->create_transient_memory_pool("GameRenderer_TransientMemoryPool");
    m_render_graph_resource_registry = std::make_unique<RenderGraphResourceRegistry>();

    constexpr uint64_t FRAME_LINEAR_ALLOCATOR_PER_FRAME_SIZE = 1024 * 1024; // 1 MiB
    m_frame_linear_allocator = std::make_unique<FrameLinearAllocator>(
        FRAMES_IN_FLIGHT, FRAME_LINEAR_ALLOCATOR_PER_FRAME_SIZE, "GameRenderer_FrameLinearAllocator");
}

GameRenderer::~GameRenderer()
{
    g_render_device->wait_idle();

    for (IRenderModule* module : m_render_modules)
    {
        delete module;
    }

    m_render_graph_builder.reset();

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_render_graphs[i].reset();

        m_fences[i].reset();
        m_image_acquired_semaphores[i].reset();
        m_render_finished_semaphores[i].reset();
    }

    m_frame_linear_allocator.reset();
    m_render_graph_resource_registry.reset();
    m_render_graph_transient_memory_pool.reset();

    m_swapchain.reset();

    ShaderManager::get().reset();
    PipelineCache::get().reset();
    SamplerStateCache::get().reset();

    delete g_render_device;
    Device::free();
}

void GameRenderer::acquire_swapchain_image()
{
    m_fences[m_current_frame]->wait_for();
    m_swapchain->acquire_next_image(m_image_acquired_semaphores[m_current_frame], nullptr);
}

void GameRenderer::set_frame_timing(const RenderFrameTiming& frame_timing)
{
    m_frame_timings[m_current_frame] = frame_timing;
}

JobHandle GameRenderer::create_update_jobs(const JobHandle& wait_job)
{
    const JobHandle prepare_frame_update_systems_batch =
        g_job_system->schedule_batch()
            .add(JobDescription::create(&GameRenderer::prepare_frame_job, this).name("PrepareFrame"))
            .add(JobDescription::create(&GameRenderer::update_systems_job, this).name("UpdateSystems"))
            .depends_on(wait_job)
            .submit();

    const JobHandle build_render_graph_job = g_job_system->schedule(&GameRenderer::build_render_graph_job, this)
                                                 .depends_on(prepare_frame_update_systems_batch)
                                                 .name("BuildRenderGraph")
                                                 .submit();

    const JobHandle compile_render_graph_prepare_draw_blocks_batch =
        g_job_system->schedule_batch()
            .add(JobDescription::create(&GameRenderer::compile_render_graph_job, this).name("CompileRenderGraph"))
            .add(JobDescription::create(&GameRenderer::prepare_draw_blocks_job, this).name("PrepareDrawBlocks"))
            .depends_on(build_render_graph_job)
            .submit();

    const JobHandle execute_and_present_job = g_job_system->schedule(&GameRenderer::execute_and_present_job, this)
                                                  .depends_on(compile_render_graph_prepare_draw_blocks_batch)
                                                  .name("ExecuteAndPresent")
                                                  .submit();

    return execute_and_present_job;
}

void GameRenderer::prepare_frame_job()
{
    MIZU_PROFILE_SCOPED;

    g_render_device->prepare_frame(m_current_frame);
    m_frame_linear_allocator->prepare_frame(m_current_frame);

    m_render_graph_builder.reset();
}

void GameRenderer::update_systems_job()
{
    MIZU_PROFILE_SCOPED;

    const Camera& camera = rend_get_camera_state();
    const RenderGraphRendererSettings& settings = rend_get_renderer_settings().settings;

    mesh_manager_update();
    light_manager_update(camera, settings.cascaded_shadows);
}

void GameRenderer::build_render_graph_job()
{
    MIZU_PROFILE_SCOPED;

    const auto& swapchain_image = m_swapchain->get_image(m_swapchain->get_current_image_idx());
    const RenderFrameTiming& frame_timing = m_frame_timings[m_current_frame];

    RenderGraphBuilder& builder = m_render_graph_builder;
    RenderGraphBlackboard blackboard{};

    FrameInfo& frame_info = blackboard.add<FrameInfo>();
    frame_info.width = swapchain_image->get_width();
    frame_info.height = swapchain_image->get_height();
    frame_info.frame_idx = m_current_frame;
    frame_info.last_frame_time = frame_timing.frame_delta_seconds;
    frame_info.frame_allocator = m_frame_linear_allocator.get();
    frame_info.output_texture = swapchain_image;
    frame_info.output_texture_ref = builder.register_external_texture(
        frame_info.output_texture,
        {.initial_state = ImageResourceState::Undefined, .final_state = ImageResourceState::Present});

    for (IRenderModule* module : m_render_modules)
    {
        if (module == nullptr)
            continue;

        module->build_render_graph(builder, blackboard);
    }
}

void GameRenderer::compile_render_graph_job()
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphBuilderCompileOptions builder_compile_options{
        *m_render_graph_transient_memory_pool, *m_render_graph_resource_registry};

    RenderGraph& render_graph = m_render_graphs[m_current_frame];
    m_render_graph_builder.compile(render_graph, builder_compile_options);
}

void GameRenderer::prepare_draw_blocks_job()
{
    MIZU_PROFILE_SCOPED;
    // TODO:
}

void GameRenderer::execute_and_present_job()
{
    MIZU_PROFILE_SCOPED;

    const auto& image_acquired_semaphore = m_image_acquired_semaphores[m_current_frame];
    const auto& render_finished_semaphore = m_render_finished_semaphores[m_current_frame];

    CommandBufferSubmitInfo submit_info{};
    submit_info.wait_semaphores = {image_acquired_semaphore};
    submit_info.signal_semaphores = {render_finished_semaphore};
    submit_info.signal_fence = m_fences[m_current_frame];

    RenderGraph& render_graph = m_render_graphs[m_current_frame];
    render_graph.execute(submit_info);

    m_swapchain->present({render_finished_semaphore});

    m_current_frame = (m_current_frame + 1) % FRAMES_IN_FLIGHT;

    MIZU_PROFILE_FRAME_MARK;
}

void setup_default_game_renderer(GameRenderer& renderer)
{
    renderer.register_module<RenderGraphRenderer>(RenderModuleLabel::Scene);
}

} // namespace Mizu
