#include "render/runtime/game_renderer.h"

#include "base/debug/profiling.h"
#include "core/window.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"

#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph_renderer.h"
#include "render/runtime/renderer.h"
#include "render/systems/shader_manager.h"

namespace Mizu
{

GameRenderer::GameRenderer(const GameRendererDescription& desc) : m_window(desc.window)
{
    for (size_t i = 0; i < m_render_modules.size(); ++i)
    {
        m_render_modules[i] = nullptr;
    }

#if MIZU_RENDER_CORE_VULKAN_ENABLED
    std::vector<const char*> vulkan_instance_extensions = m_window->get_vulkan_instance_extensions();
#endif

    ApiSpecificConfiguration specific_config;
    switch (desc.graphics_api)
    {
#if MIZU_RENDER_CORE_DX12_ENABLED
    case GraphicsApi::Dx12:
        specific_config = Dx12SpecificConfiguration{};
        break;
#endif
#if MIZU_RENDER_CORE_VULKAN_ENABLED
    case GraphicsApi::Vulkan:
        specific_config = VulkanSpecificConfiguration{
            .binding_offsets = VulkanBindingOffsets{},
            .instance_extensions = vulkan_instance_extensions,
        };
        break;
#endif
    }

    DeviceCreationDescription config{};
    config.api = desc.graphics_api;
    config.specific_config = specific_config;
    config.application_name = desc.application_name;
    config.application_version = desc.application_version;
    config.engine_name = "MizuEngine";
    config.engine_version = Version{0, 1, 0};

    g_render_device = Device::create(config);

    ShaderManager::get().add_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_PATH);

    SwapchainDescription swapchain_desc{};
    swapchain_desc.window = m_window;
    swapchain_desc.format = ImageFormat::R8G8B8A8_SRGB;

    m_swapchain = g_render_device->create_swapchain(swapchain_desc);

    m_current_frame = 0;
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_command_buffers[i] = g_render_device->create_command_buffer(CommandBufferType::Graphics);

        m_fences[i] = g_render_device->create_fence();
        m_image_acquired_semaphores[i] = g_render_device->create_semaphore();
        m_render_finished_semaphores[i] = g_render_device->create_semaphore();
    }
}

GameRenderer::~GameRenderer()
{
    g_render_device->wait_idle();

    for (IRenderModule* module : m_render_modules)
    {
        delete module;
    }

    delete g_render_device;
}

void GameRenderer::render()
{
    MIZU_PROFILE_SCOPED;

    m_fences[m_current_frame]->wait_for();

    CommandBuffer& command_buffer = *m_command_buffers[m_current_frame];
    const auto& image_acquired_semaphore = m_image_acquired_semaphores[m_current_frame];
    const auto& render_finished_semaphore = m_render_finished_semaphores[m_current_frame];

    m_swapchain->acquire_next_image(image_acquired_semaphore, nullptr);

    RenderGraphBuilder builder{};
    RenderGraphBlackboard blackboard{};

    for (IRenderModule* module : m_render_modules)
    {
        if (module == nullptr)
            continue;

        module->build_render_graph(builder, blackboard);
    }

    const RenderGraphBuilderMemory builder_memory =
        RenderGraphBuilderMemory{*m_render_graph_transient_allocator, *m_render_graph_host_allocators[m_current_frame]};

    RenderGraph& render_graph = m_render_graphs[m_current_frame];
    builder.compile(render_graph, builder_memory);

    CommandBufferSubmitInfo submit_info{};
    submit_info.wait_semaphores = {image_acquired_semaphore};
    submit_info.signal_semaphores = {render_finished_semaphore};
    submit_info.signal_fence = m_fences[m_current_frame];

    render_graph.execute(command_buffer, submit_info);

    m_swapchain->present({render_finished_semaphore});

    m_current_frame = (m_current_frame + 1) % FRAMES_IN_FLIGHT;
}

void setup_default_game_renderer(GameRenderer& renderer)
{
    renderer.register_module<RenderGraphRenderer>(RenderModuleLabel::Scene);
}

} // namespace Mizu