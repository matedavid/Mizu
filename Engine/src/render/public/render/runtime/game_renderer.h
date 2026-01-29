#pragma once

#include "base/containers/inplace_vector.h"
#include "render_core/rhi/device.h"

#include "mizu_render_module.h"
#include "render/render_graph/render_graph.h"

namespace Mizu
{

// Forward declarations
class AliasedDeviceMemoryAllocator;
class CommandBuffer;
class Fence;
class RenderGraphBlackboard;
class RenderGraphBuilder;
class Semaphore;
class Swapchain;
class Window;

struct GameRendererDescription
{
    GraphicsApi graphics_api;
    std::shared_ptr<Window> window;

    std::string application_name;
    Version application_version;
};

enum class RenderModuleLabel
{
    Bottom,
    Scene,
    Top,

    Count,
};

class IRenderModule
{
  public:
    virtual ~IRenderModule() = default;

    virtual void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) = 0;
};

class MIZU_RENDER_API GameRenderer
{
  public:
    GameRenderer(const GameRendererDescription& desc);
    ~GameRenderer();

    void render();

    template <typename T>
    void register_module(RenderModuleLabel label)
    {
        static_assert(std::is_base_of_v<IRenderModule, T>, "T must derive from IRenderModule");
        MIZU_ASSERT(label != RenderModuleLabel::Count, "Can't register module with label Count");

        const size_t idx = static_cast<size_t>(label);
        if (m_render_modules[idx] != nullptr)
        {
            delete m_render_modules[idx];
        }

        m_render_modules[idx] = new T{};
    }

  private:
    std::shared_ptr<Window> m_window{};
    std::array<IRenderModule*, static_cast<size_t>(RenderModuleLabel::Count)> m_render_modules{};

    static constexpr size_t FRAMES_IN_FLIGHT = 1; // TEMPORAL !!!

    double m_current_time = 0.0;

    uint32_t m_current_frame = 0;
    std::shared_ptr<Swapchain> m_swapchain{};
    std::array<RenderGraph, FRAMES_IN_FLIGHT> m_render_graphs{};
    std::array<std::shared_ptr<CommandBuffer>, FRAMES_IN_FLIGHT> m_command_buffers{};
    std::array<std::shared_ptr<Fence>, FRAMES_IN_FLIGHT> m_fences{};
    std::array<std::shared_ptr<Semaphore>, FRAMES_IN_FLIGHT> m_image_acquired_semaphores{};
    std::array<std::shared_ptr<Semaphore>, FRAMES_IN_FLIGHT> m_render_finished_semaphores{};

    std::shared_ptr<AliasedDeviceMemoryAllocator> m_render_graph_transient_allocator{};
    // We need one host allocator per frame in flight because we could be have the case where one frame is reading the
    // host memory, and the other frame is writing into the memory compiling the next RenderGraph.
    // This does not happen for the transient memory because we only bind the memory on compilation, writing happens
    // when copying the staging buffer or in a gpu operation.
    std::array<std::shared_ptr<AliasedDeviceMemoryAllocator>, FRAMES_IN_FLIGHT> m_render_graph_host_allocators{};
};

MIZU_RENDER_API void setup_default_game_renderer(GameRenderer& renderer);

} // namespace Mizu