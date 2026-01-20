#include "render/core/command_utils.h"

#include "render_core/rhi/synchronization.h"

#include "render/renderer.h"

namespace Mizu
{

void CommandUtils::submit_single_time(CommandBufferType type, const SubmitSingleTimeFunc& func)
{
    const auto fence = g_render_device->create_fence(false);

    auto command = g_render_device->create_command_buffer(type);
    command->begin();

    func(*command);

    command->end();

    CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = fence;

    command->submit(submit_info);

    fence->wait_for();
}

} // namespace Mizu
