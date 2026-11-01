#include "vulkan_queue.h"

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "vulkan_command_buffer.h"
#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_synchronization.h"

namespace Mizu::Vulkan
{

VkSemaphoreSubmitInfo vulkan_semaphore_submit_info(VkSemaphore semaphore, uint64_t value)
{
    VkSemaphoreSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    info.semaphore = semaphore;
    info.value = value;
    info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    info.deviceIndex = 0;

    return info;
}

VkSemaphoreSubmitInfo vulkan_semaphore_submit_info(const SyncPoint& point)
{
    MIZU_ASSERT(point.is_valid(), "Can't create semaphore submit info from an invalid SyncPoint");
    return vulkan_semaphore_submit_info(static_cast<const VulkanFence*>(point.fence)->handle(), point.value);
}

VulkanQueue::VulkanQueue(VkQueue queue, uint32_t queue_family, CommandBufferType type)
    : m_handle(queue)
    , m_family(queue_family)
    , m_type(type)
{
    m_fence = std::make_unique<VulkanFence>(0);
}

VulkanQueue::~VulkanQueue() = default;

SyncPoint VulkanQueue::submit(std::span<CommandBuffer*> command_buffers, const SubmitInfo& info)
{
    MIZU_PROFILE_SCOPED;

    constexpr size_t MAX_COMMAND_BUFFERS = 16;
    MIZU_ASSERT(
        command_buffers.size() <= MAX_COMMAND_BUFFERS,
        "Too many command buffers in a single submit ({} > {})",
        command_buffers.size(),
        MAX_COMMAND_BUFFERS);

    inplace_vector<VkCommandBufferSubmitInfo, MAX_COMMAND_BUFFERS> command_buffer_infos;
    for (const CommandBuffer* command_buffer : command_buffers)
    {
        MIZU_ASSERT(
            &VulkanContext.device->get_native_queue(command_buffer->get_type()) == this,
            "Command buffer can't be submitted to this queue");

        VkCommandBufferSubmitInfo command_buffer_info{};
        command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_info.commandBuffer = static_cast<const VulkanCommandBuffer*>(command_buffer)->handle();
        command_buffer_info.deviceMask = 0;

        command_buffer_infos.push_back(command_buffer_info);
    }

    inplace_vector<VkSemaphoreSubmitInfo, SubmitInfo::MAX_SIGNALS + 1> signals;

    const std::lock_guard lock(m_mutex);

    for (const SyncPoint& wait : info.waits)
    {
        if (wait.is_valid())
            m_pending_waits.push_back(vulkan_semaphore_submit_info(wait));
    }

    m_last_value += 1;
    signals.push_back(vulkan_semaphore_submit_info(m_fence->handle(), m_last_value));

    for (const SyncPoint& signal : info.signals)
        signals.push_back(vulkan_semaphore_submit_info(signal));

    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(m_pending_waits.size());
    submit_info.pWaitSemaphoreInfos = m_pending_waits.data();
    submit_info.commandBufferInfoCount = static_cast<uint32_t>(command_buffer_infos.size());
    submit_info.pCommandBufferInfos = command_buffer_infos.data();
    submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signals.size());
    submit_info.pSignalSemaphoreInfos = signals.data();

    VK_CHECK(vkQueueSubmit2(m_handle, 1, &submit_info, VK_NULL_HANDLE));

    m_pending_waits.clear();

    return SyncPoint{m_fence.get(), m_last_value};
}

void VulkanQueue::wait(const SyncPoint& point)
{
    if (!point.is_valid())
        return;

    const std::lock_guard lock(m_mutex);
    m_pending_waits.push_back(vulkan_semaphore_submit_info(point));
}

SyncPoint VulkanQueue::get_last_submitted() const
{
    const std::lock_guard lock(m_mutex);
    return SyncPoint{m_fence.get(), m_last_value};
}

void VulkanQueue::wait_idle()
{
    MIZU_PROFILE_SCOPED;

    const std::lock_guard lock(m_mutex);
    VK_CHECK(vkQueueWaitIdle(m_handle));
}

void VulkanQueue::submit_interop(
    std::span<const VkSemaphoreSubmitInfo> waits,
    std::span<const VkSemaphoreSubmitInfo> signals)
{
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(waits.size());
    submit_info.pWaitSemaphoreInfos = waits.data();
    submit_info.commandBufferInfoCount = 0;
    submit_info.pCommandBufferInfos = nullptr;
    submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signals.size());
    submit_info.pSignalSemaphoreInfos = signals.data();

    const std::lock_guard lock(m_mutex);
    VK_CHECK(vkQueueSubmit2(m_handle, 1, &submit_info, VK_NULL_HANDLE));
}

VkResult VulkanQueue::present(const VkPresentInfoKHR& info)
{
    const std::lock_guard lock(m_mutex);
    return vkQueuePresentKHR(m_handle, &info);
}

} // namespace Mizu::Vulkan
