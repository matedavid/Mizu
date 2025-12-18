#pragma once

#include "render_core/rhi/synchronization.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

class VulkanFence : public Fence
{
  public:
    VulkanFence(bool signaled);
    ~VulkanFence() override;

    void wait_for() override;

    VkFence handle() const { return m_handle; }

  private:
    VkFence m_handle{VK_NULL_HANDLE};
};

class VulkanSemaphore : public Semaphore
{
  public:
    VulkanSemaphore();
    ~VulkanSemaphore() override;

    VkSemaphore handle() const { return m_handle; }

  private:
    VkSemaphore m_handle{VK_NULL_HANDLE};
};

} // namespace Mizu::Vulkan
