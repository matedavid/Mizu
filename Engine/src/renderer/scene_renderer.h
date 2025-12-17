#pragma once

#include <array>
#include <memory>

#define MIZU_USE_IMGUI MIZU_DEBUG && 0

#if MIZU_USE_IMGUI
#include "extensions/imgui/imgui_presenter.h"
#endif

#include "render_core/render_graph/render_graph.h"
#include "renderer/render_graph_renderer.h"

namespace Mizu
{

// Forward declarations
class AliasedDeviceMemoryAllocator;
class CommandBuffer;
class Fence;
class RenderGraph;
class Semaphore;
class Swapchain;

class SceneRenderer
{
  public:
    SceneRenderer();
    ~SceneRenderer();

    void render();

  private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    uint32_t m_current_frame = 0;

    std::array<RenderGraphRenderer, MAX_FRAMES_IN_FLIGHT> m_renderers;
    std::array<std::shared_ptr<CommandBuffer>, MAX_FRAMES_IN_FLIGHT> m_command_buffers;
    std::array<RenderGraph, MAX_FRAMES_IN_FLIGHT> m_render_graphs;

    std::array<std::shared_ptr<Fence>, MAX_FRAMES_IN_FLIGHT> m_fences;
    std::array<std::shared_ptr<Semaphore>, MAX_FRAMES_IN_FLIGHT> m_image_acquired_semaphores;
    std::array<std::shared_ptr<Semaphore>, MAX_FRAMES_IN_FLIGHT> m_render_finished_semaphores;

#if MIZU_USE_IMGUI
    std::unique_ptr<ImGuiPresenter> m_imgui_presenter;
    std::array<std::shared_ptr<Texture2D>, MAX_FRAMES_IN_FLIGHT> m_output_textures;
    std::array<std::shared_ptr<ShaderResourceView>, MAX_FRAMES_IN_FLIGHT> m_output_image_views;
    std::array<ImTextureID, MAX_FRAMES_IN_FLIGHT> m_output_imgui_textures;
#else
    std::shared_ptr<Swapchain> m_swapchain;
#endif

    std::shared_ptr<AliasedDeviceMemoryAllocator> m_render_graph_transient_allocator;
    // We need one host allocator per frame in flight because we could be have the case where one frame is reading the
    // host memory, and the other frame is writing into the memory compiling the next RenderGraph.
    // This does not happen for the transient memory because we only bind the memory on compilation, writing happens
    // when copying the staging buffer or in a gpu operation.
    std::array<std::shared_ptr<AliasedDeviceMemoryAllocator>, MAX_FRAMES_IN_FLIGHT> m_render_graph_host_allocators;
};

} // namespace Mizu
