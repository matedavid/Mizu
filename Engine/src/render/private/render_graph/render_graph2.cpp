#include "render/render_graph/render_graph2.h"

#include <variant>

#include "base/debug/profiling.h"

#include "render/render_graph/render_graph_builder2.h"

namespace Mizu
{

void RenderGraph2::execute(const CommandBufferSubmitInfo& submit_info)
{
    MIZU_PROFILE_SCOPED;

    insert_external_submit_info(submit_info);

    for (const CommandBufferBatch& batch : m_command_buffer_batches)
    {
        auto& command_buffer = batch.command_buffer;

        command_buffer->begin();

        for (const RenderGraphCmd& cmd : batch.commands)
        {
            std::visit([&](const auto& concrete_cmd) { execute_internal(*command_buffer, concrete_cmd); }, cmd);
        }

        command_buffer->end();
    }

    for (const CommandBufferBatch& batch : m_command_buffer_batches)
    {
        batch.command_buffer->submit(batch.submit_info);
    }
}

void RenderGraph2::execute()
{
    execute(CommandBufferSubmitInfo{});
}

void RenderGraph2::reset()
{
    MIZU_PROFILE_SCOPED;

    m_command_buffer_batches.clear();
    m_pass_resources.clear();
}

void RenderGraph2::insert_external_submit_info(const CommandBufferSubmitInfo& submit_info)
{
    if (submit_info.wait_semaphores.empty() && submit_info.signal_semaphores.empty()
        && submit_info.signal_fence == nullptr)
    {
        return;
    }

    for (CommandBufferBatch& batch : m_command_buffer_batches)
    {
        if (batch.incoming_batch_indices.empty())
        {
            batch.submit_info.wait_semaphores.insert(
                batch.submit_info.wait_semaphores.end(),
                submit_info.wait_semaphores.begin(),
                submit_info.wait_semaphores.end());
        }
    }

    for (int64_t i = static_cast<int64_t>(m_command_buffer_batches.size()) - 1; i >= 0; --i)
    {
        CommandBufferBatch& batch = m_command_buffer_batches[i];

        if (batch.outgoing_batch_indices.empty())
        {
            batch.submit_info.signal_semaphores.insert(
                batch.submit_info.signal_semaphores.end(),
                submit_info.signal_semaphores.begin(),
                submit_info.signal_semaphores.end());

            if (submit_info.signal_fence)
            {
                batch.submit_info.signal_fence = submit_info.signal_fence;
            }
        }
    }
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