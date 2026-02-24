#include "render/render_graph/render_graph2.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"

#include "render/render_graph/render_graph_builder2.h"

namespace Mizu
{

void RenderGraph2::execute()
{
    MIZU_PROFILE_SCOPED;

    size_t i = 0;
    for (const CommandBufferBatch& batch : m_command_buffer_batches)
    {
        MIZU_LOG_INFO("CommandBuffer: {} (type={})", i, static_cast<size_t>(batch.type));

        for (const RenderGraphCmd& cmd : batch.commands)
        {
            std::visit(
                [&](const auto& concrete_cmd) {
                    using CmdType = std::decay_t<decltype(concrete_cmd)>;
                    if constexpr (std::derived_from<CmdType, BufferTransitionCmd>)
                    {
                        const BufferTransitionCmd& transition_cmd = concrete_cmd;
                        MIZU_LOG_INFO(
                            "Executing buffer transition command: buffer={}, {} -> {} ({})",
                            transition_cmd.resource.get_name(),
                            buffer_resource_state_to_string(transition_cmd.initial),
                            buffer_resource_state_to_string(transition_cmd.final),
                            resource_transition_mode_to_string(transition_cmd.transition_mode));
                    }
                    else if constexpr (std::derived_from<CmdType, ImageTransitionCmd>)
                    {
                        const ImageTransitionCmd& transition_cmd = concrete_cmd;
                        MIZU_LOG_INFO(
                            "Executing image transition command: image={}, {} -> {} ({})",
                            transition_cmd.resource.get_name(),
                            image_resource_state_to_string(transition_cmd.initial),
                            image_resource_state_to_string(transition_cmd.final),
                            resource_transition_mode_to_string(transition_cmd.transition_mode));
                    }
                    else if constexpr (std::derived_from<CmdType, PassExecuteCmd>)
                    {
                        MIZU_LOG_INFO("Executing pass command");
                    }
                },
                cmd);
        }

        MIZU_LOG_INFO("===================================\n");
        i += 1;
    }
}

void RenderGraph2::reset()
{
    MIZU_PROFILE_SCOPED;

    m_command_buffer_batches.clear();
    m_pass_resources.clear();
}

} // namespace Mizu