#pragma once

#include <vulkan/vulkan.h>

#include "renderer/abstraction/synchronization.h"

namespace Mizu::Vulkan {

class VulkanFence : public Fence {
  public:
    VulkanFence();
    ~VulkanFence() override;

    void wait_for() const override;

    [[nodiscard]] VkFence handle() const { return m_handle; }

  private:
    VkFence m_handle{VK_NULL_HANDLE};
};

class VulkanSemaphore : public Semaphore {
  public:
    VulkanSemaphore();
    ~VulkanSemaphore() override;

    [[nodiscard]] VkSemaphore handle() const { return m_handle; }

  private:
    VkSemaphore m_handle{VK_NULL_HANDLE};
};

} // namespace Mizu::Vulkan
