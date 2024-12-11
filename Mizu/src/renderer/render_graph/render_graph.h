#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "renderer/abstraction/command_buffer.h"

#include "renderer/render_graph/render_graph_types.h"

namespace Mizu
{

// Forward declarations
class RenderGraphDeviceMemoryAllocator;
class ResourceGroup;

using RGPassFunc = std::function<void(RenderCommandBuffer&)>;

class RenderGraph
{
  public:
    RenderGraph() = default;
    RenderGraph(std::shared_ptr<RenderCommandBuffer> command_buffer);

    void execute(const CommandBufferSubmitInfo& submit_info) const;

  private:
    std::shared_ptr<RenderCommandBuffer> m_command_buffer;
    std::vector<RGPassFunc> m_passes;

    friend class RenderGraphBuilder;
};

} // namespace Mizu
