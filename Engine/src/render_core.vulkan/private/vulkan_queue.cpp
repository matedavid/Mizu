#include "vulkan_queue.h"

#include <vector>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "vulkan_buffer_resource.h"
#include "vulkan_command_buffer.h"
#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_gpu_heap.h"
#include "vulkan_image_resource.h"
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

static VkSparseMemoryBind get_sparse_memory_bind(
    const MemoryPlacement& placement,
    uint64_t resource_offset,
    uint64_t size)
{
    const VulkanGpuHeap& native_heap = static_cast<const VulkanGpuHeap&>(*placement.heap);

    VkSparseMemoryBind sparse_memory_bind{};
    sparse_memory_bind.resourceOffset = resource_offset;
    sparse_memory_bind.size = size;
    sparse_memory_bind.memory = native_heap.handle();
    sparse_memory_bind.memoryOffset = placement.offset;
    sparse_memory_bind.flags = 0; // TODO

    return sparse_memory_bind;
}

static VkSparseImageMemoryBind get_sparse_image_memory_bind(const SparseImageMapping& mapping)
{
    const VulkanGpuHeap& native_heap = static_cast<const VulkanGpuHeap&>(*mapping.memory.heap);

    VkSparseImageMemoryBind sparse_image_memory_bind{};
    sparse_image_memory_bind.subresource = VkImageSubresource{
        .aspectMask = 0, // TODO
        .mipLevel = mapping.mip_level,
        .arrayLayer = mapping.array_layer,
    };
    sparse_image_memory_bind.offset = VkOffset3D{
        .x = static_cast<int32_t>(mapping.offset.x),
        .y = static_cast<int32_t>(mapping.offset.y),
        .z = static_cast<int32_t>(mapping.offset.z),
    };
    sparse_image_memory_bind.extent = VkExtent3D{
        .width = mapping.extent.x,
        .height = mapping.extent.y,
        .depth = mapping.extent.z,
    };
    sparse_image_memory_bind.memory = native_heap.handle();
    sparse_image_memory_bind.memoryOffset = mapping.memory.offset;
    sparse_image_memory_bind.flags = 0; // TODO

    return sparse_image_memory_bind;
}

SyncPoint VulkanQueue::bind_sparse(const SparseBindBatch& batch, const SubmitInfo& info)
{
    const std::span<const SparseBufferMapping> buffer_mappings = batch.get_buffer_mappings();
    const std::span<const SparseImageMapping> image_mappings = batch.get_image_mappings();

    // TODO: Temporary allocations :(
    std::vector<VkSparseMemoryBind> sparse_memory_binds{};
    std::vector<VkSparseImageMemoryBind> sparse_image_memory_binds{};

    std::vector<VkSparseBufferMemoryBindInfo> buffer_bind_infos{};
    std::vector<VkSparseImageMemoryBindInfo> image_bind_infos{};

    for (const SparseBufferMapping& mapping : buffer_mappings)
    {
        const VulkanBufferResource& native_buffer = static_cast<const VulkanBufferResource&>(*mapping.buffer);

        VkSparseMemoryBind& sparse_memory_bind = sparse_memory_binds.emplace_back();
        sparse_memory_bind = get_sparse_memory_bind(mapping.memory, mapping.resource_offset, mapping.size);

        VkSparseBufferMemoryBindInfo& buffer_bind_info = buffer_bind_infos.emplace_back();
        buffer_bind_info.buffer = native_buffer.handle();
        buffer_bind_info.bindCount = 1;
        buffer_bind_info.pBinds = &sparse_memory_bind;
    }

    for (const SparseImageMapping& mapping : image_mappings)
    {
        const VulkanImageResource& native_image = static_cast<const VulkanImageResource&>(*mapping.image);

        VkSparseImageMemoryBind& image_bind = sparse_image_memory_binds.emplace_back();
        image_bind = get_sparse_image_memory_bind(mapping);

        VkSparseImageMemoryBindInfo& bind_info = image_bind_infos.emplace_back();
        bind_info.image = native_image.handle();
        bind_info.bindCount = 1;
        bind_info.pBinds = &image_bind;
    }

    inplace_vector<VkSemaphoreSubmitInfo, SubmitInfo::MAX_SIGNALS + 1> signals;

    const std::lock_guard lock(m_mutex);

    for (const SyncPoint& wait : info.waits)
    {
        m_pending_waits.push_back(vulkan_semaphore_submit_info(wait));
    }

    m_last_value += 1;
    signals.push_back(vulkan_semaphore_submit_info(m_fence->handle(), m_last_value));

    for (const SyncPoint& signal : info.signals)
    {
        signals.push_back(vulkan_semaphore_submit_info(signal));
    }

    std::vector<VkSemaphore> wait_semaphores{};
    std::vector<uint64_t> wait_values{};

    std::vector<VkSemaphore> signal_semaphores{};
    std::vector<uint64_t> signal_values{};

    wait_semaphores.reserve(m_pending_waits.size());
    wait_values.reserve(m_pending_waits.size());

    signal_semaphores.reserve(signals.size());
    signal_values.reserve(signals.size());

    for (const VkSemaphoreSubmitInfo& wait : m_pending_waits)
    {
        wait_semaphores.push_back(wait.semaphore);
        wait_values.push_back(wait.value);
    }

    for (const VkSemaphoreSubmitInfo& signal : signals)
    {
        signal_semaphores.push_back(signal.semaphore);
        signal_values.push_back(signal.value);
    }

    VkTimelineSemaphoreSubmitInfo timeline_info{};
    timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline_info.waitSemaphoreValueCount = static_cast<uint32_t>(wait_values.size());
    timeline_info.pWaitSemaphoreValues = wait_values.data();
    timeline_info.signalSemaphoreValueCount = static_cast<uint32_t>(signal_values.size());
    timeline_info.pSignalSemaphoreValues = signal_values.data();

    VkBindSparseInfo bind_sparse_info{};
    bind_sparse_info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
    bind_sparse_info.pNext = &timeline_info;
    bind_sparse_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
    bind_sparse_info.pWaitSemaphores = wait_semaphores.data();
    bind_sparse_info.bufferBindCount = static_cast<uint32_t>(buffer_bind_infos.size());
    bind_sparse_info.pBufferBinds = buffer_bind_infos.data();
    bind_sparse_info.imageOpaqueBindCount = 0;
    bind_sparse_info.pImageOpaqueBinds = nullptr;
    bind_sparse_info.imageBindCount = static_cast<uint32_t>(image_bind_infos.size());
    bind_sparse_info.pImageBinds = image_bind_infos.data();
    bind_sparse_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
    bind_sparse_info.pSignalSemaphores = signal_semaphores.data();

    VK_CHECK(vkQueueBindSparse(m_handle, 1, &bind_sparse_info, VK_NULL_HANDLE));

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
