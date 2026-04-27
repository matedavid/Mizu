#pragma once

#include "render_core/rhi/device.h"

#include "mizu_render_module.h"
#include "render/render_graph/render_graph.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph/render_graph_resource_registry.h"
#include "render/runtime/render_frame_timing.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class Fence;
class FrameLinearAllocator;
class RenderGraphBlackboard;
class Semaphore;
class Swapchain;
class TransientMemoryPool;
class Window;
struct JobHandle;

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

    GameRenderer(const GameRenderer&) = delete;
    GameRenderer& operator=(const GameRenderer&) = delete;

    void acquire_swapchain_image();
    void set_frame_timing(const RenderFrameTiming& frame_timing);
    JobHandle create_update_jobs(const JobHandle& wait_job);

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

    static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    uint32_t m_current_frame = 0;
    std::shared_ptr<Swapchain> m_swapchain{};

    std::array<std::shared_ptr<Fence>, FRAMES_IN_FLIGHT> m_fences{};
    std::array<std::shared_ptr<Semaphore>, FRAMES_IN_FLIGHT> m_image_acquired_semaphores{};
    std::array<std::shared_ptr<Semaphore>, FRAMES_IN_FLIGHT> m_render_finished_semaphores{};
    std::array<RenderFrameTiming, FRAMES_IN_FLIGHT> m_frame_timings{};

    RenderGraphBuilder m_render_graph_builder{};
    std::array<RenderGraph, FRAMES_IN_FLIGHT> m_render_graphs{};
    std::shared_ptr<TransientMemoryPool> m_render_graph_transient_memory_pool{};
    std::unique_ptr<RenderGraphResourceRegistry> m_render_graph_resource_registry{};
    std::unique_ptr<FrameLinearAllocator> m_frame_linear_allocator{};

    void prepare_frame_job();
    void update_systems_job();
    void build_render_graph_job();
    void compile_render_graph_job();
    void prepare_draw_blocks_job();
    void execute_and_present_job();
};

MIZU_RENDER_API void setup_default_game_renderer(GameRenderer& renderer);

} // namespace Mizu
