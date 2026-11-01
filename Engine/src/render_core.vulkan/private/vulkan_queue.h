#pragma once

#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "render_core/rhi/queue.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

class VulkanFence;

VkSemaphoreSubmitInfo vulkan_semaphore_submit_info(VkSemaphore semaphore, uint64_t value);
VkSemaphoreSubmitInfo vulkan_semaphore_submit_info(const SyncPoint& point);

class VulkanQueue : public Queue
{
  public:
    VulkanQueue(VkQueue queue, uint32_t queue_family, CommandBufferType type);
    ~VulkanQueue() override;

    CommandBufferType get_type() const override { return m_type; }

    using Queue::submit;
    SyncPoint submit(std::span<CommandBuffer*> command_buffers, const SubmitInfo& info) override;

    void wait(const SyncPoint& point) override;

    SyncPoint get_last_submitted() const override;
    void wait_idle() override;

    // Swapchain interop with binary semaphores. Never signals the queue fence, as empty batches may complete out of
    // order
    void submit_interop(std::span<const VkSemaphoreSubmitInfo> waits, std::span<const VkSemaphoreSubmitInfo> signals);
    VkResult present(const VkPresentInfoKHR& info);

    [[nodiscard]] VkQueue handle() const { return m_handle; }
    [[nodiscard]] uint32_t family() const { return m_family; }

  private:
    VkQueue m_handle;
    uint32_t m_family;
    CommandBufferType m_type;

    std::unique_ptr<VulkanFence> m_fence;
    uint64_t m_last_value = 0;

    std::vector<VkSemaphoreSubmitInfo> m_pending_waits;

    mutable std::mutex m_mutex;
};

} // namespace Mizu::Vulkan
