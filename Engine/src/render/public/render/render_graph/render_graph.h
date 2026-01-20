#pragma once

#include <functional>
#include <vector>

#include "render_core/rhi/command_buffer.h"

#include "render/render_graph/render_graph_types.h"
#include "mizu_render_module.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;

class MIZU_RENDER_API RenderGraph
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
