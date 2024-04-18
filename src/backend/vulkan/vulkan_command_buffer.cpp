#include "vulkan_command_buffer.h"

#include "backend/vulkan/vk_core.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_device.h"
#include "backend/vulkan/vulkan_queue.h"
#include "backend/vulkan/vulkan_synchronization.h"

namespace Mizu::Vulkan {

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::VulkanCommandBufferBase() {
    const auto cbs = VulkanContext.device->allocate_command_buffers(1, Type);
    assert(!cbs.empty() && "Error allocating command buffers");

    m_command_buffer = cbs[0];
}

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::~VulkanCommandBufferBase() {
    VulkanContext.device->free_command_buffers({m_command_buffer}, Type);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::begin() const {
    VK_CHECK(vkResetCommandBuffer(m_command_buffer, 0));

    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(m_command_buffer, &info));
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::end() const {
    VK_CHECK(vkEndCommandBuffer(m_command_buffer));
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit() const {
    submit({});
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit(const CommandBufferSubmitInfo& info) const {
    std::vector<VkSemaphore> wait_semaphores;
    if (info.wait_semaphore != nullptr) {
        const auto wait_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(info.wait_semaphore);
        wait_semaphores.push_back(wait_semaphore->handle());
    }

    std::vector<VkSemaphore> signal_semaphores;
    if (info.signal_semaphore != nullptr) {
        const auto signal_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(info.signal_semaphore);
        signal_semaphores.push_back(signal_semaphore->handle());
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = nullptr;
    submit_info.pWaitDstStageMask = nullptr;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;
    submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();

    VkFence signal_fence = info.signal_fence != nullptr
                               ? std::dynamic_pointer_cast<VulkanFence>(info.signal_fence)->handle()
                               : VK_NULL_HANDLE;

    get_queue()->submit(submit_info, signal_fence);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit_single_time(
    const std::function<void(const VulkanCommandBufferBase<Type>&)>& func) {
    const VulkanCommandBufferBase<Type> command_buffer{};

    command_buffer.begin();

    func(command_buffer);

    command_buffer.end();

    command_buffer.submit();

    vkQueueWaitIdle(get_queue()->handle());
}

template <CommandBufferType Type>
std::shared_ptr<VulkanQueue> VulkanCommandBufferBase<Type>::get_queue() {
    switch (Type) {
    case CommandBufferType::Graphics:
        return VulkanContext.device->get_graphics_queue();
    case CommandBufferType::Compute:
        return VulkanContext.device->get_compute_queue();
    case CommandBufferType::Transfer:
        return VulkanContext.device->get_transfer_queue();
    }
}

template class VulkanCommandBufferBase<CommandBufferType::Graphics>;
template class VulkanCommandBufferBase<CommandBufferType::Compute>;
template class VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
