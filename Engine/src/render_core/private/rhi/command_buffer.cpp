#include "render_core/rhi/command_buffer.h"

#include "render_core/rhi/synchronization.h"

namespace Mizu
{

void CommandBuffer::submit() const
{
    submit(CommandBufferSubmitInfo{});
}

} // namespace Mizu
