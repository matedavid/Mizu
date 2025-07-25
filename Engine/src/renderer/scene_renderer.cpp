#include "scene_renderer.h"

#include "application/application.h"
#include "application/window.h"

#include "base/debug/profiling.h"

#include "renderer/camera.h"

#include "render_core/render_graph/render_graph_builder.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"

#include "state_manager/camera_state_manager.h"

namespace Mizu
{

SceneRenderer::SceneRenderer()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_command_buffers[i] = RenderCommandBuffer::create();

        m_fences[i] = Fence::create();
        m_image_acquired_semaphores[i] = Semaphore::create();
        m_render_finished_semaphores[i] = Semaphore::create();
    }

    m_swapchain = Swapchain::create(Application::instance()->get_window());
    m_render_graph_allocator = RenderGraphDeviceMemoryAllocator::create();

    m_current_frame = 0;
}

SceneRenderer::~SceneRenderer()
{
    Renderer::wait_idle();
}

void SceneRenderer::render()
{
    MIZU_PROFILE_SCOPE_NAMED("SceneRenderer::render");

    m_fences[m_current_frame]->wait_for();

    CommandBuffer& command_buffer = *m_command_buffers[m_current_frame];
    const auto& image_acquired_semaphore = m_image_acquired_semaphores[m_current_frame];
    const auto& render_finished_semaphore = m_render_finished_semaphores[m_current_frame];

    m_swapchain->acquire_next_image(image_acquired_semaphore, nullptr);
    const std::shared_ptr<Texture2D>& texture = m_swapchain->get_image(m_swapchain->get_current_image_idx());

    const Camera& camera = rend_get_camera_state();

    RenderGraphBuilder builder;

    builder.begin_gpu_marker("SceneRenderer");
    {
        m_renderers[m_current_frame].build(builder, camera, *texture);
    }
    builder.end_gpu_marker();

    RenderGraph& render_graph = m_render_graphs[m_current_frame];
    render_graph = builder.compile(*m_render_graph_allocator);

    Mizu::CommandBufferSubmitInfo submit_info{};
    submit_info.wait_semaphore = image_acquired_semaphore;
    submit_info.signal_semaphore = render_finished_semaphore;
    submit_info.signal_fence = m_fences[m_current_frame];

    render_graph.execute(command_buffer, submit_info);

    m_swapchain->present({render_finished_semaphore});

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace Mizu