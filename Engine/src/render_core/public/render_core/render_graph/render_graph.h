#pragma once

#include <functional>
#include <vector>

#include "mizu_render_core_module.h"
#include "render_core/render_graph/render_graph_types.h"
#include "render_core/rhi/command_buffer.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;

class MIZU_RENDER_CORE_API RenderGraph
{
  public:
    RenderGraph() = default;

    void execute(CommandBuffer& command_buffer, const CommandBufferSubmitInfo& submit_info) const;

    void reset();

  private:
    using RGInternalFunction = std::function<void(CommandBuffer&)>;
    std::vector<RGInternalFunction> m_passes;

    RGResourceGroupMap m_resource_group_map;

    friend class RenderGraphBuilder;
};

} // namespace Mizu
