#pragma once

#include <glm/glm.hpp>
#include <unordered_map>

#include "render/runtime/game_renderer.h"
#include "render/systems/frame_linear_allocator.h"

#include "imgui/imgui_state_manager.h"
#include "imgui/imgui_types.h"
#include "mizu_imgui_render_module.h"

namespace Mizu
{

class MIZU_IMGUI_RENDER_API ImGuiRenderModule : public IRenderModule, public ImGuiStateManagerConsumer
{
  public:
    bool init() override;
    void shutdown() override;

    JobHandle create_update_jobs(const RenderModuleUpdateContext& ctx) override;

    void build_render_graph(
        RenderGraphBuilder& builder,
        RenderGraphBlackboard& blackboard,
        const RenderModuleFrameData& frame_data) override;

    // ImGuiStateManagerConsumer
    void rend_on_create(ImGuiHandle handle, const ImGuiStaticState& ss, const ImGuiDynamicState& ds) override;
    void rend_on_update(ImGuiHandle handle, const ImGuiDynamicState& ds) override;
    void rend_on_destroy(ImGuiHandle handle) override;

  private:
    struct ImGuiFrameData
    {
        ImGuiHandle handle{};
        const ImGuiFrameSnapshot* snapshot = nullptr;
    };

    std::vector<ImGuiFrameData> m_frame_data_list{};

    struct ImGuiDrawData
    {
        glm::vec2 scale{};
        glm::vec2 translate{};

        glm::vec2 clip_min{};
        glm::vec2 clip_max{};

        uint32_t vertex_offset = 0;
        uint32_t index_offset = 0;
        uint32_t bindless_slot = 0;

        uint32_t _padding = 0;
    };

    std::vector<DrawIndirectCommand> m_draw_commands{};
    std::atomic<uint32_t> m_debug_num_draw_commands{};

    struct PendingTextureOp
    {
        ImageResource* resource = nullptr;
        ImGuiTextureOp op{};
    };

    std::vector<PendingTextureOp> m_pending_texture_ops{};

    void process_texture_ops(const ImGuiFrameSnapshot* snapshot);
    void record_pending_texture_ops(CommandBuffer& command, FrameLinearAllocator& frame_allocator);
};

} // namespace Mizu