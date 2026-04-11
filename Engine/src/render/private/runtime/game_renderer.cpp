#include "render/runtime/game_renderer.h"

#include "base/debug/profiling.h"
#include "core/runtime.h"
#include "core/window.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"

#include "render/frame_linear_allocator.h"
#include "render/passes/pass_info.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph_renderer.h"
#include "render/runtime/renderer.h"
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

    constexpr uint64_t FRAME_LINEAR_ALLOCATOR_FRAME_SIZE = 1024 * 1024; // 1 MiB
    m_frame_linear_allocator = std::make_unique<FrameLinearAllocator>(
        FRAMES_IN_FLIGHT, FRAME_LINEAR_ALLOCATOR_FRAME_SIZE, "GameRenderer_FrameLinearAllocator");
}

GameRenderer::~GameRenderer()
{
    g_render_device->wait_idle();

    for (IRenderModule* module : m_render_modules)
    {
        delete module;
    }

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

JobSystemHandle GameRenderer::create_update_jobs(JobSystemHandle wait_job)
{
    m_render_graph_builder.reset();

    const Job prepare_frame_job = Job::create([&] {
                                      MIZU_PROFILE_SCOPED;

                                      g_render_device->prepare_frame(m_current_frame);
                                      m_frame_linear_allocator->prepare_frame(m_current_frame);

                                      m_render_graph_builder.reset();
                                  }).depends_on(wait_job);
    const JobSystemHandle prepare_frame_job_handle = g_job_system->schedule(prepare_frame_job);

    const Job update_systems_job = Job::create(&GameRenderer::update_systems_job, this).depends_on(wait_job);
    const JobSystemHandle update_systems_job_handle = g_job_system->schedule(update_systems_job);

    const Job build_render_graph_job = Job::create(&GameRenderer::build_render_graph_job, this)
                                           .depends_on(prepare_frame_job_handle)
                                           .depends_on(update_systems_job_handle);
    const JobSystemHandle build_render_graph_job_handle = g_job_system->schedule(build_render_graph_job);

    const Job compile_render_graph_job =
        Job::create(&GameRenderer::compile_render_graph_job, this).depends_on(build_render_graph_job_handle);

    const Job prepare_draw_jobs_job =
        Job::create(&GameRenderer::prepare_draw_blocks_job, this).depends_on(build_render_graph_job_handle);

    const std::array compile_render_graph_prepare_draw_jobs = {compile_render_graph_job, prepare_draw_jobs_job};
    const JobSystemHandle compile_and_prepare_draw_jobs_handle =
        g_job_system->schedule(compile_render_graph_prepare_draw_jobs);

    const Job execute_and_present_job =
        Job::create(&GameRenderer::execute_and_present_job, this).depends_on(compile_and_prepare_draw_jobs_handle);
    const JobSystemHandle execute_and_present_job_handle = g_job_system->schedule(execute_and_present_job);

    return execute_and_present_job_handle;
}

void GameRenderer::render()
{
    MIZU_PROFILE_SCOPED;

    m_fences[m_current_frame]->wait_for();

    const double current_time = m_window->get_current_time();
    const double dt = current_time - m_current_time;
    m_current_time = current_time;

    g_render_device->prepare_frame(m_current_frame);
    m_frame_linear_allocator->prepare_frame(m_current_frame);

    const auto& image_acquired_semaphore = m_image_acquired_semaphores[m_current_frame];
    const auto& render_finished_semaphore = m_render_finished_semaphores[m_current_frame];

    m_swapchain->acquire_next_image(image_acquired_semaphore, nullptr);
    const auto& swapchain_image = m_swapchain->get_image(m_swapchain->get_current_image_idx());

    RenderGraphBuilder builder{};
    RenderGraphBlackboard blackboard{};

    FrameInfo& frame_info = blackboard.add<FrameInfo>();
    frame_info.width = swapchain_image->get_width();
    frame_info.height = swapchain_image->get_height();
    frame_info.frame_idx = m_current_frame;
    frame_info.last_frame_time = dt;
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

    const RenderGraphBuilderCompileOptions builder_compile_options{
        *m_render_graph_transient_memory_pool, *m_render_graph_resource_registry};

    RenderGraph& render_graph = m_render_graphs[m_current_frame];
    builder.compile(render_graph, builder_compile_options);

    CommandBufferSubmitInfo submit_info{};
    submit_info.wait_semaphores = {image_acquired_semaphore};
    submit_info.signal_semaphores = {render_finished_semaphore};
    submit_info.signal_fence = m_fences[m_current_frame];

    render_graph.execute(submit_info);

    m_swapchain->present({render_finished_semaphore});

    m_current_frame = (m_current_frame + 1) % FRAMES_IN_FLIGHT;
}

void GameRenderer::update_systems_job()
{
    MIZU_PROFILE_SCOPED;

    // Mesh, texture, light... managers and other systems
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
    frame_info.output_texture_ref = m_render_graph_builder.register_external_texture(
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
