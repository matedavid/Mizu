#include "vulkan_queue.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

VulkanQueue::VulkanQueue(VkQueue queue, uint32_t queue_family) : m_handle(queue), m_family(queue_family) {}

void VulkanQueue::submit(VkSubmitInfo2 info, VkFence fence) const
{
    submit(&info, 1, fence);
}

void VulkanQueue::submit(VkSubmitInfo2* info, uint32_t submit_count, VkFence fence) const
{
    const std::lock_guard lock(m_submit_mutex);
    VK_CHECK(vkQueueSubmit2(m_handle, submit_count, info, fence));
}

} // namespace Mizu::Vulkan
