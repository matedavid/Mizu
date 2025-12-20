#include "scene_renderer.h"

#include <format>

#include "application/application.h"
#include "application/thread_sync.h"
#include "application/window.h"

#include "base/debug/profiling.h"

#include "render/camera.h"
#include "render/render_graph/render_graph_builder.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device_memory_allocator.h"
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
    SwapchainDescription swapchain_desc{};
    swapchain_desc.window = Application::instance()->get_window();
    swapchain_desc.format = ImageFormat::R8G8B8A8_SRGB;

    m_swapchain = g_render_device->create_swapchain(swapchain_desc);
#endif

    m_render_graph_transient_allocator =
        g_render_device->create_aliased_memory_allocator(false, "SceneRenderer_TransientAllocator");

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_command_buffers[i] = g_render_device->create_command_buffer(CommandBufferType::Graphics);

        const std::string host_allocator_name = std::format("SceneRenderer_HostAllocator_{}", i);
        m_render_graph_host_allocators[i] = g_render_device->create_aliased_memory_allocator(true, host_allocator_name);

        m_fences[i] = g_render_device->create_fence();
        m_image_acquired_semaphores[i] = g_render_device->create_semaphore();
        m_render_finished_semaphores[i] = g_render_device->create_semaphore();

#if MIZU_USE_IMGUI
        Texture2D::Description output_texture_desc{};
        output_texture_desc.dimensions = {
            Application::instance()->get_window()->get_width(),
            Application::instance()->get_window()->get_height(),
        };
        output_texture_desc.format = ImageFormat::B8G8R8A8_SRGB;
        output_texture_desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled;
        output_texture_desc.name = std::format("ImGuiOutput_{}", i);

        m_output_textures[i] = Texture2D::create(output_texture_desc);
        m_output_image_views[i] = ShaderResourceView::create(m_output_textures[i]->get_resource());
        m_output_imgui_textures[i] = m_imgui_presenter->add_texture(*m_output_image_views[i]);
#endif
    }

    m_current_frame = 0;
}

SceneRenderer::~SceneRenderer()
{
    g_render_device->wait_idle();
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
    const JobSystemHandle acquire_next_image_job_handle = g_job_system->schedule(acquire_next_image_job);

    const std::shared_ptr<Texture2D>& texture = m_output_textures[m_current_frame];
    const ImTextureID& output_imgui_texture = m_output_imgui_textures[m_current_frame];
#else
    m_swapchain->acquire_next_image(image_acquired_semaphore, nullptr);
    const std::shared_ptr<ImageResource>& texture = m_swapchain->get_image(m_swapchain->get_current_image_idx());
#endif

    const Camera& camera = rend_get_camera_state();

    RenderGraphBuilder builder;

    builder.begin_gpu_marker("SceneRenderer");
    {
        m_renderers[m_current_frame].build(builder, camera, texture);
    }
    builder.end_gpu_marker();

    const RenderGraphBuilderMemory builder_memory =
        RenderGraphBuilderMemory{*m_render_graph_transient_allocator, *m_render_graph_host_allocators[m_current_frame]};

    RenderGraph& render_graph = m_render_graphs[m_current_frame];
    builder.compile(render_graph, builder_memory);

#if MIZU_USE_IMGUI
    acquire_next_image_job_handle.wait();
#endif

    CommandBufferSubmitInfo submit_info{};
    submit_info.wait_semaphores = {image_acquired_semaphore};
    submit_info.signal_semaphores = {render_finished_semaphore};
    submit_info.signal_fence = m_fences[m_current_frame];

    render_graph.execute(command_buffer, submit_info);

#if MIZU_USE_IMGUI
    rend_execute_imgui_function();

    m_imgui_presenter->set_background_texture(output_imgui_texture);
    m_imgui_presenter->render_imgui_and_present({render_finished_semaphore});
#else
    m_swapchain->present({render_finished_semaphore});
#endif

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace Mizu