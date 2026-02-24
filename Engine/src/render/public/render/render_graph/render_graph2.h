#pragma once

#include <vector>

namespace Mizu
{

// Forward declarations
class RenderGraphPassResources2;
struct CommandBufferBatch;

class RenderGraph2
{
  public:
    void execute();

    void reset();

  private:
    friend class RenderGraphBuilder2;

    std::vector<CommandBufferBatch> m_command_buffer_batches;
    std::vector<RenderGraphPassResources2> m_pass_resources;
};

} // namespace Mizu