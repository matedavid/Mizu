#include "vulkan_synchronization.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"

namespace Mizu::Vulkan {

//
// VulkanFence
//

VulkanFence::VulkanFence() {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(VulkanContext.device->handle(), &info, nullptr, &m_handle));
}

VulkanFence::~VulkanFence() {
    vkDestroyFence(VulkanContext.device->handle(), m_handle, nullptr);
}

void VulkanFence::wait_for() const {
    vkWaitForFences(VulkanContext.device->handle(), 1, &m_handle, VK_TRUE, UINT64_MAX);
    vkResetFences(VulkanContext.device->handle(), 1, &m_handle);
}

//
// VulkanSemaphore
//

VulkanSemaphore::VulkanSemaphore() {
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(vkCreateSemaphore(VulkanContext.device->handle(), &info, nullptr, &m_handle));
}

VulkanSemaphore::~VulkanSemaphore() {
    vkDestroySemaphore(VulkanContext.device->handle(), m_handle, nullptr);
}

} // namespace Mizu::Vulkan
