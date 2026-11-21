#include "render/utils/command_utils.h"

#include "render_core/rhi/queue.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

void CommandUtils::submit_single_time(CommandBufferType type, const SubmitSingleTimeFunc& func)
{
    auto command = g_render_device->create_command_buffer(type);
    command->begin();

    func(*command);

    command->end();

    g_render_device->get_queue(type).submit(*command).wait();
}

} // namespace Mizu
