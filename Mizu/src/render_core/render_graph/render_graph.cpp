#include "render_graph.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

RenderGraph::RenderGraph(std::shared_ptr<RenderCommandBuffer> command_buffer)
    : m_command_buffer(std::move(command_buffer))
{
}

void RenderGraph::execute(const CommandBufferSubmitInfo& submit_info) const
{
    m_command_buffer->begin();

    for (const RGFunction& func : m_passes)
    {
        func(*m_command_buffer);
    }

    m_command_buffer->end();

    m_command_buffer->submit(submit_info);
}

} // namespace Mizu
