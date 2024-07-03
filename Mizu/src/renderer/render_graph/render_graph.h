#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "renderer/render_graph/render_graph_builder.h"

namespace Mizu {

// Forward declarations
class RenderGraphBuilder;
class RenderCommandBuffer;
class RenderPass;
class GraphicsPipeline;
struct CommandBufferSubmitInfo;

class RenderGraph {
  public:
    static std::optional<RenderGraph> build(const RenderGraphBuilder& builder);

    void execute(const CommandBufferSubmitInfo& submit_info) const;

  private:
    std::shared_ptr<RenderCommandBuffer> m_command_buffer;

    struct RGRenderPass {
        std::shared_ptr<RenderPass> render_pass;
        std::shared_ptr<GraphicsPipeline> graphics_pipeline;
        RGFunction func;
    };
    std::vector<RGRenderPass> m_render_passes;
};

} // namespace Mizu
