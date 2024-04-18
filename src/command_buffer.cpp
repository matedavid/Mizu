#include "command_buffer.h"

#include "renderer.h"

#include "backend/vulkan/vulkan_command_buffer.h"

namespace Mizu {

std::shared_ptr<RenderCommandBuffer> RenderCommandBuffer::create() {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRenderCommandBuffer>();
    }
}

std::shared_ptr<ComputeCommandBuffer> ComputeCommandBuffer::create() {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanComputeCommandBuffer>();
    }
}

} // namespace Mizu
