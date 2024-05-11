#include "vulkan_queue.h"

#include "backend/vulkan/vk_core.h"

namespace Mizu::Vulkan {

VulkanQueue::VulkanQueue(VkQueue queue, uint32_t queue_family) : m_handle(queue), m_family(queue_family) {}

void VulkanQueue::submit(VkSubmitInfo info, VkFence fence) const {
    submit(&info, 1, fence);
}

void VulkanQueue::submit(VkSubmitInfo* info, uint32_t submit_count, VkFence fence) const {
    VK_CHECK(vkQueueSubmit(m_handle, submit_count, info, fence));
}

} // namespace Mizu::Vulkan
