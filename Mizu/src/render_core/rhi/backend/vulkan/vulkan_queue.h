#pragma once

#include <vulkan/vulkan.h>

namespace Mizu::Vulkan
{

class VulkanQueue
{
  public:
    VulkanQueue(VkQueue queue, uint32_t queue_family);
    ~VulkanQueue() = default;

    void submit(VkSubmitInfo info, VkFence fence = VK_NULL_HANDLE) const;
    void submit(VkSubmitInfo* info, uint32_t submit_count, VkFence fence = VK_NULL_HANDLE) const;

    [[nodiscard]] VkQueue handle() const { return m_handle; }
    [[nodiscard]] uint32_t family() const { return m_family; }

  private:
    VkQueue m_handle;
    uint32_t m_family;
};

} // namespace Mizu::Vulkan
