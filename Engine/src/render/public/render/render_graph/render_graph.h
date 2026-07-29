#pragma once

#include <vector>

#include "mizu_render_module.h"
#include "render/render_graph/render_graph_builder.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;
struct CommandBufferSubmitInfo;

class MIZU_RENDER_API RenderGraph
{
  public:
    RenderGraph() = default;

    RenderGraph(const RenderGraph& other) = delete;
    RenderGraph& operator=(const RenderGraph& other) = delete;

    RenderGraph(RenderGraph&& other) = default;
    RenderGraph& operator=(RenderGraph&& other) = default;

    void execute(const CommandBufferSubmitInfo& submit_info);
    void execute();

    void reset();

  private:
    friend class RenderGraphBuilder;

    void insert_external_submit_info(const CommandBufferSubmitInfo& submit_info);

    void execute_internal(CommandBuffer& command, const BufferTransitionCmd& cmd);
    void execute_internal(CommandBuffer& command, const ImageTransitionCmd& cmd);
    void execute_internal(CommandBuffer& command, const AccelStructTransitionCmd& cmd);
    void execute_internal(CommandBuffer& command, const PassExecuteCmd& cmd);

    std::vector<CommandBufferBatch> m_command_buffer_batches;
    std::vector<RenderGraphPassResources> m_pass_resources;
};

} // namespace Mizu
