#include "buffers.h"

#include "backend/vulkan/vulkan_buffers.h"

namespace Mizu {

std::shared_ptr<IndexBuffer> IndexBuffer::create(const std::vector<uint32_t>& data) {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanIndexBuffer>(data);
    }
}

} // namespace Mizu