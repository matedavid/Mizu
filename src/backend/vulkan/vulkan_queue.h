#pragma once

#include <vulkan/vulkan.h>

namespace Mizu::Vulkan {

class VulkanQueue {
  public:
    explicit VulkanQueue(VkQueue queue);
    ~VulkanQueue() = default;

    void submit(VkSubmitInfo info, VkFence fence = VK_NULL_HANDLE) const;
    void submit(VkSubmitInfo* info, uint32_t submit_count, VkFence fence = VK_NULL_HANDLE) const;

  private:
    VkQueue m_handle;
};

} // namespace Mizu::Vulkan
