#include "render_graph.h"

#include "base/debug/profiling.h"

namespace Mizu
{

void RenderGraph::execute(CommandBuffer& command_buffer, const CommandBufferSubmitInfo& submit_info) const
{
    MIZU_PROFILE_SCOPED;

    command_buffer.begin();

    for (const RGInternalFunction& func : m_passes)
    {
        func(command_buffer);
    }

    command_buffer.end();

    command_buffer.submit(submit_info);
}

void RenderGraph::reset()
{
    MIZU_PROFILE_SCOPED;

    m_resource_group_map.clear();
    m_passes.clear();
}

} // namespace Mizu
