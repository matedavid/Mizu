#include "command_buffer.h"

#include "render_core/rhi/renderer.h"
#include "render_core/rhi/synchronization.h"

#include "render_core/rhi/backend/opengl/opengl_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"

namespace Mizu
{

std::shared_ptr<CommandBuffer> CommandBuffer::create(CommandBufferType type)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanCommandBuffer>(type);
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Not implemented");
        return nullptr;
    }
}

void CommandBuffer::submit_single_time(CommandBufferType type, const SubmitSingleTimeFunc& func)
{
    const auto fence = Fence::create();
    fence->wait_for(); // Fences start signaled by default

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
