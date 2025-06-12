#include "render_graph.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

void RenderGraph::execute(CommandBuffer& command_buffer, const CommandBufferSubmitInfo& submit_info) const
{
    command_buffer.begin();

    for (const RGInternalFunction& func : m_passes)
    {
        func(command_buffer);
    }

    command_buffer.end();

    command_buffer.submit(submit_info);
}

} // namespace Mizu
