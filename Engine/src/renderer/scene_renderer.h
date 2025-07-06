#pragma once

#include <array>
#include <memory>

#include "render_core/render_graph/render_graph.h"

namespace Mizu
{

// Forward declarations
class Camera;
class Texture2D;
class Semaphore;
struct CommandBufferSubmitInfo;

class ISceneRenderer
{
  public:
    virtual ~ISceneRenderer() = default;

    virtual void render(const Camera& camera, const Texture2D& output, const CommandBufferSubmitInfo& submit_info) = 0;
};

// Forward declarations
class CommandBuffer;
class Fence;
class RenderGraph;
class RenderGraphDeviceMemoryAllocator;
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

    std::array<std::shared_ptr<CommandBuffer>, MAX_FRAMES_IN_FLIGHT> m_command_buffers;
    std::array<RenderGraph, MAX_FRAMES_IN_FLIGHT> m_render_graphs;

    std::array<std::shared_ptr<Fence>, MAX_FRAMES_IN_FLIGHT> m_fences;
    std::array<std::shared_ptr<Semaphore>, MAX_FRAMES_IN_FLIGHT> m_image_acquired_semaphores;
    std::array<std::shared_ptr<Semaphore>, MAX_FRAMES_IN_FLIGHT> m_render_finished_semaphores;

    std::shared_ptr<Swapchain> m_swapchain;

    std::shared_ptr<RenderGraphDeviceMemoryAllocator> m_render_graph_allocator;
};

} // namespace Mizu
