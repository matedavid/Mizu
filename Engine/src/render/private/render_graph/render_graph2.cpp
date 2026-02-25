#include "render/render_graph/render_graph2.h"

#include <variant>

#include "base/debug/profiling.h"

#include "render/render_graph/render_graph_builder2.h"

namespace Mizu
{

void RenderGraph2::execute()
{
    MIZU_PROFILE_SCOPED;

    for (const CommandBufferBatch& batch : m_command_buffer_batches)
    {
        auto command_buffer = batch.command_buffer;

        command_buffer->begin();

        for (const RenderGraphCmd& cmd : batch.commands)
        {
            std::visit([&](const auto& concrete_cmd) { execute_internal(*command_buffer, concrete_cmd); }, cmd);
        }

        command_buffer->end();
    }
}

void RenderGraph2::reset()
{
    MIZU_PROFILE_SCOPED;

    m_command_buffer_batches.clear();
    m_pass_resources.clear();
}

void RenderGraph2::execute_internal(CommandBuffer& command, const BufferTransitionCmd& cmd)
{
    const BufferTransitionInfo transition_info{
        cmd.initial,
        cmd.final,
        0,
        cmd.resource.get_size(),
        cmd.src_queue_type,
        cmd.dst_queue_type,
        cmd.transition_mode};

    command.transition_resource(cmd.resource, transition_info);
}

void RenderGraph2::execute_internal(CommandBuffer& command, const ImageTransitionCmd& cmd)
{
    const ImageTransitionInfo transition_info{
        cmd.initial,
        cmd.final,
        ImageResourceViewDescription{},
        cmd.src_queue_type,
        cmd.dst_queue_type,
        cmd.transition_mode};

    command.transition_resource(cmd.resource, transition_info);
}

void RenderGraph2::execute_internal(CommandBuffer& command, const PassExecuteCmd& cmd)
{
    command.begin_gpu_marker(cmd.name);
    cmd.func(command, cmd.resources);
    command.end_gpu_marker();
}

} // namespace Mizu