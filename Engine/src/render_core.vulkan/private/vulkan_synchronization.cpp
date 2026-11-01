#include "vulkan_synchronization.h"

#include "base/debug/profiling.h"

#include "vulkan_context.h"
#include "vulkan_core.h"

namespace Mizu::Vulkan
{

//
// VulkanFence
//

VulkanFence::VulkanFence(uint64_t initial_value)
{
    VkSemaphoreTypeCreateInfo type_info{};
    type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type_info.initialValue = initial_value;

    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = &type_info;

    VK_CHECK(vkCreateSemaphore(VulkanContext.device->handle(), &info, nullptr, &m_handle));
}

VulkanFence::~VulkanFence()
{
    vkDestroySemaphore(VulkanContext.device->handle(), m_handle, nullptr);
}

uint64_t VulkanFence::get_completed_value() const
{
    uint64_t value = 0;
    VK_CHECK(vkGetSemaphoreCounterValue(VulkanContext.device->handle(), m_handle, &value));
    return value;
}

void VulkanFence::wait(uint64_t value) const
{
    MIZU_PROFILE_SCOPED;

    VkSemaphoreWaitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    info.semaphoreCount = 1;
    info.pSemaphores = &m_handle;
    info.pValues = &value;

    VK_CHECK(vkWaitSemaphores(VulkanContext.device->handle(), &info, UINT64_MAX));
}

void VulkanFence::signal(uint64_t value)
{
    VkSemaphoreSignalInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    info.semaphore = m_handle;
    info.value = value;

    VK_CHECK(vkSignalSemaphore(VulkanContext.device->handle(), &info));
}

} // namespace Mizu::Vulkan
