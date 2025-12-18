#include "render_core/rhi/command_buffer.h"

#include "render_core/rhi/renderer.h"
#include "render_core/rhi/synchronization.h"

namespace Mizu
{

std::shared_ptr<CommandBuffer> CommandBuffer::create(CommandBufferType type)
{
    (void)type;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

void CommandBuffer::submit_single_time(CommandBufferType type, const SubmitSingleTimeFunc& func)
{
    const auto fence = Fence::create(false);

    auto command = CommandBuffer::create(type);
    command->begin();

    func(*command);

    command->end();

    CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = fence;

    command->submit(submit_info);

    fence->wait_for();
}

void CommandBuffer::submit() const
{
    submit(CommandBufferSubmitInfo{});
}

} // namespace Mizu
