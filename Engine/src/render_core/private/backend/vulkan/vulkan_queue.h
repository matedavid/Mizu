#pragma once

#include <mutex>

#include "backend/vulkan/vulkan_core.h"

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

    // Is mutable because the submit method is const, and I don't want to change it to non-const
    mutable std::mutex m_submit_mutex;
};

} // namespace Mizu::Vulkan
