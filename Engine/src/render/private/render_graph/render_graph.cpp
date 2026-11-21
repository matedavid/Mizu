#include "render/render_graph/render_graph.h"

#include <ranges>
#include <variant>

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "render/render_graph/render_graph_builder.h"
#include "render/runtime/renderer.h"

namespace Mizu
{

SyncPoint RenderGraph::execute(const SubmitInfo& submit_info)
{
    MIZU_PROFILE_SCOPED;

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

    size_t final_batch_idx = m_command_buffer_batches.size();
    for (const CommandBufferBatch& batch : m_command_buffer_batches | std::views::reverse)
    {
        if (batch.outgoing_batch_indices.empty())
        {
            final_batch_idx = batch.idx;
            break;
        }
    }

    std::vector<SyncPoint> batch_sync_points(m_command_buffer_batches.size());
    SyncPoint final_sync_point{};

    for (const CommandBufferBatch& batch : m_command_buffer_batches)
    {
        SubmitInfo batch_submit_info{};

        if (batch.incoming_batch_indices.empty())
            batch_submit_info.waits = submit_info.waits;

        for (size_t incoming_batch_idx : batch.incoming_batch_indices)
        {
            // Batches on the same queue are ordered by submission order and their barriers
            if (m_command_buffer_batches[incoming_batch_idx].type == batch.type)
                continue;

            MIZU_ASSERT(
                batch_sync_points[incoming_batch_idx].is_valid(),
                "Batch {} depends on batch {}, which has not been submitted yet",
                batch.idx,
                incoming_batch_idx);

            batch_submit_info.waits.push_back(batch_sync_points[incoming_batch_idx]);
        }

        if (batch.idx == final_batch_idx)
            batch_submit_info.signals = submit_info.signals;

        Queue& queue = g_render_device->get_queue(batch.type);
        batch_sync_points[batch.idx] = queue.submit(*batch.command_buffer, batch_submit_info);

        if (batch.idx == final_batch_idx)
            final_sync_point = batch_sync_points[batch.idx];
    }

    return final_sync_point;
}

void RenderGraph::reset()
{
    MIZU_PROFILE_SCOPED;

    m_command_buffer_batches.clear();
    m_pass_resources.clear();
}

void RenderGraph::execute_internal(CommandBuffer& command, const BufferTransitionCmd& cmd)
{
    const BufferTransitionInfo transition_info{
        cmd.resource,
        cmd.initial,
        cmd.final,
        cmd.resource.get_size(),
        0,
        cmd.src_queue_type,
        cmd.dst_queue_type,
        cmd.transition_mode,
    };

    command.transition_resource(transition_info);
}

void RenderGraph::execute_internal(CommandBuffer& command, const ImageTransitionCmd& cmd)
{
    const ImageTransitionInfo transition_info{
        cmd.resource,
        cmd.initial,
        cmd.final,
        ImageResourceViewDescription{},
        cmd.src_queue_type,
        cmd.dst_queue_type,
        cmd.transition_mode,
    };

    command.transition_resource(transition_info);
}

void RenderGraph::execute_internal(CommandBuffer& command, const AccelStructTransitionCmd& cmd)
{
    const AccelerationStructureTransitionInfo transition_info{
        cmd.resource,
        cmd.initial,
        cmd.final,
        cmd.src_queue_type,
        cmd.dst_queue_type,
        cmd.transition_mode,
    };

    command.transition_resource(transition_info);
}

void RenderGraph::execute_internal(CommandBuffer& command, const ResourceTransitionBatchCmd& cmd)
{
    std::vector<ResourceTransitionInfoT> infos{};
    infos.reserve(cmd.transitions.size());

    struct CreateTransitionInfoOperator
    {
        BufferTransitionInfo operator()(const BufferTransitionCmd& cmd)
        {
            return {
                cmd.resource,
                cmd.initial,
                cmd.final,
                cmd.resource.get_size(),
                0,
                cmd.src_queue_type,
                cmd.dst_queue_type,
                cmd.transition_mode,
            };
        }

        ImageTransitionInfo operator()(const ImageTransitionCmd& cmd)
        {
            return {
                cmd.resource,
                cmd.initial,
                cmd.final,
                ImageResourceViewDescription{},
                cmd.src_queue_type,
                cmd.dst_queue_type,
                cmd.transition_mode,
            };
        }

        AccelerationStructureTransitionInfo operator()(const AccelStructTransitionCmd& cmd)
        {
            return {
                cmd.resource,
                cmd.initial,
                cmd.final,
                cmd.src_queue_type,
                cmd.dst_queue_type,
                cmd.transition_mode,
            };
        }
    };

    CreateTransitionInfoOperator create_transition_info{};

    for (const ResourceTransitionBatchCmd::TransitionCmdT& transition : cmd.transitions)
    {
        std::visit([&](const auto& value) { infos.push_back(create_transition_info(value)); }, transition);
    }

    command.transition_resources(infos);
}

void RenderGraph::execute_internal(CommandBuffer& command, const PassExecuteCmd& cmd)
{
    command.begin_gpu_marker(cmd.name);
    cmd.func(command, m_pass_resources[cmd.pass_resources_idx]);
    command.end_gpu_marker();
}

} // namespace Mizu
