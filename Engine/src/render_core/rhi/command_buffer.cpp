#include "command_buffer.h"

#include "render_core/rhi/renderer.h"
#include "render_core/rhi/synchronization.h"

#include "render_core/rhi/backend/directx12/dx12_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"

namespace Mizu
{

std::shared_ptr<CommandBuffer> CommandBuffer::create(CommandBufferType type)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12CommandBuffer>(type);
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanCommandBuffer>(type);
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
