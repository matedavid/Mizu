#include "scene_renderer.h"

#include "application/application.h"
#include "application/thread_sync.h"
#include "application/window.h"

#include "base/debug/profiling.h"

#include "renderer/camera.h"

#include "render_core/render_graph/render_graph_builder.h"

#include "render_core/resources/texture.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"

#include "state_manager/camera_state_manager.h"
#include "state_manager/imgui_state_manager.h"

namespace Mizu
{

SceneRenderer::SceneRenderer()
{
#if MIZU_USE_IMGUI
    m_imgui_presenter = std::make_unique<ImGuiPresenter>(Application::instance()->get_window());
#else
    m_swapchain = Swapchain::create(Application::instance()->get_window());
#endif

    m_render_graph_transient_allocator = AliasedDeviceMemoryAllocator::create();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_command_buffers[i] = RenderCommandBuffer::create();
        m_render_graph_host_allocators[i] = AliasedDeviceMemoryAllocator::create(true);

        m_fences[i] = Fence::create();
        m_image_acquired_semaphores[i] = Semaphore::create();
        m_render_finished_semaphores[i] = Semaphore::create();

#if MIZU_USE_IMGUI
        Texture2D::Description output_texture_desc{};
        output_texture_desc.dimensions = {
            Application::instance()->get_window()->get_width(), Application::instance()->get_window()->get_height()};
        output_texture_desc.format = ImageFormat::BGRA8_SRGB;
        output_texture_desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled;
        output_texture_desc.name = std::format("ImGuiOutput_{}", i);

        m_output_textures[i] = Texture2D::create(output_texture_desc);
        m_output_image_views[i] = ImageResourceView::create(m_output_textures[i]->get_resource());
        m_output_imgui_textures[i] = m_imgui_presenter->add_texture(*m_output_image_views[i]);
#endif
    }

    m_current_frame = 0;
}

SceneRenderer::~SceneRenderer()
{
    Renderer::wait_idle();
}

void SceneRenderer::render()
{
    MIZU_PROFILE_SCOPED;

    m_fences[m_current_frame]->wait_for();

    CommandBuffer& command_buffer = *m_command_buffers[m_current_frame];
    const auto& image_acquired_semaphore = m_image_acquired_semaphores[m_current_frame];
    const auto& render_finished_semaphore = m_render_finished_semaphores[m_current_frame];

#if MIZU_USE_IMGUI
    // Setting affinity to Main thread because the GLFW implementation NewFrame function calls some functions that need
    // to be ran on the same thread the window was created.
    const Job acquire_next_image_job = Job::create([=, this]() {
                                           MIZU_PROFILE_SCOPED_NAME("imgui_acquire_next_image_job");

                                           m_imgui_presenter->acquire_next_image(image_acquired_semaphore, nullptr);
                                       }).set_affinity(ThreadAffinity_Main);
    g_job_system->schedule(acquire_next_image_job).wait();

    const std::shared_ptr<Texture2D>& texture = m_output_textures[m_current_frame];
#else
    m_swapchain->acquire_next_image(image_acquired_semaphore, nullptr);
    const std::shared_ptr<Texture2D>& texture = m_swapchain->get_image(m_swapchain->get_current_image_idx());
#endif

    const Camera& camera = rend_get_camera_state();

    RenderGraphBuilder builder;

    builder.begin_gpu_marker("SceneRenderer");
    {
        m_renderers[m_current_frame].build(builder, camera, *texture);
    }
    builder.end_gpu_marker();

    const RenderGraphBuilderMemory builder_memory =
        RenderGraphBuilderMemory{*m_render_graph_transient_allocator, *m_render_graph_host_allocators[m_current_frame]};

    RenderGraph& render_graph = m_render_graphs[m_current_frame];
    builder.compile(render_graph, builder_memory);

    CommandBufferSubmitInfo submit_info{};
    submit_info.wait_semaphore = image_acquired_semaphore;
    submit_info.signal_semaphore = render_finished_semaphore;
    submit_info.signal_fence = m_fences[m_current_frame];

    render_graph.execute(command_buffer, submit_info);

#if MIZU_USE_IMGUI
    rend_execute_imgui_function();

    m_imgui_presenter->set_background_texture(m_output_imgui_textures[m_current_frame]);
    m_imgui_presenter->render_imgui_and_present({m_render_finished_semaphores[m_current_frame]});
#else
    m_swapchain->present({render_finished_semaphore});
#endif

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace Mizu