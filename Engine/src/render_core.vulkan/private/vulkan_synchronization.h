#pragma once

#include "render_core/rhi/synchronization.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

class VulkanFence : public Fence
{
  public:
    VulkanFence(uint64_t initial_value);
    ~VulkanFence() override;

    uint64_t get_completed_value() const override;
    void wait(uint64_t value) const override;
    void signal(uint64_t value) override;

    VkSemaphore handle() const { return m_handle; }

  private:
    VkSemaphore m_handle{VK_NULL_HANDLE};
};

} // namespace Mizu::Vulkan
