#pragma once

#include <vector>

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class RenderGraphPassResources2;
struct BufferTransitionCmd;
struct CommandBufferBatch;
struct ImageTransitionCmd;
struct PassExecuteCmd;

class RenderGraph2
{
  public:
    void execute();

    void reset();

  private:
    friend class RenderGraphBuilder2;

    void execute_internal(CommandBuffer& command, const BufferTransitionCmd& cmd);
    void execute_internal(CommandBuffer& command, const ImageTransitionCmd& cmd);
    void execute_internal(CommandBuffer& command, const PassExecuteCmd& cmd);

    std::vector<CommandBufferBatch> m_command_buffer_batches;
    std::vector<RenderGraphPassResources2> m_pass_resources;
};

} // namespace Mizu