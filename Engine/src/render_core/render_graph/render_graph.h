#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "render_core/rhi/command_buffer.h"

#include "render_core/render_graph/render_graph_types.h"

namespace Mizu
{

// Forward declarations
class RenderGraphDeviceMemoryAllocator;
class ResourceGroup;

class RenderGraph
{
  public:
    RenderGraph() = default;

    void execute(RenderCommandBuffer& command_buffer, const CommandBufferSubmitInfo& submit_info) const;

  private:
    std::vector<RGFunction> m_passes;

    friend class RenderGraphBuilder;
};

} // namespace Mizu
