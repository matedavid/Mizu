#include "vulkan_queue.h"

namespace Mizu::Vulkan {

VulkanQueue::VulkanQueue(VkQueue queue) : m_handle(queue) {
}

void VulkanQueue::submit(VkSubmitInfo info, VkFence fence) const {
    submit(&info, 1, fence);
}

void VulkanQueue::submit(VkSubmitInfo* info, uint32_t submit_count, VkFence fence) const {
    vkQueueSubmit(m_handle, submit_count, info, fence);

}

} // namespace Mizu::Vulkan
