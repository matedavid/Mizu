#pragma once

#include <functional>
#include <vector>

#include "render_core/render_graph/render_graph_types.h"
#include "render_core/rhi/command_buffer.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;

class RenderGraph
{
  public:
    RenderGraph() = default;

    void execute(CommandBuffer& command_buffer, const CommandBufferSubmitInfo& submit_info) const;

  private:
    using RGInternalFunction = std::function<void(CommandBuffer&)>;
    std::vector<RGInternalFunction> m_passes;

    friend class RenderGraphBuilder;
};

} // namespace Mizu
