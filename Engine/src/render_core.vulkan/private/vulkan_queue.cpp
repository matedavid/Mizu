#include "vulkan_queue.h"

#include <algorithm>
#include <ranges>
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

namespace
{

VkDeviceMemory get_vulkan_heap_memory(const GpuHeap* heap)
{
    MIZU_ASSERT(heap != nullptr, "Sparse bind requires a valid heap");
    return static_cast<const VulkanGpuHeap*>(heap)->handle();
}

VkSparseMemoryBind get_vulkan_sparse_memory_bind(
    uint64_t resource_offset,
    uint64_t size,
    VkDeviceMemory memory,
    uint64_t memory_offset,
    VkSparseMemoryBindFlags flags = 0)
{
    VkSparseMemoryBind bind{};
    bind.resourceOffset = resource_offset;
    bind.size = size;
    bind.memory = memory;
    bind.memoryOffset = memory_offset;
    bind.flags = flags;

    return bind;
}

VkSparseImageMemoryBind get_vulkan_sparse_image_memory_bind(
    const ImageResource& image,
    uint32_t mip_level,
    uint32_t array_layer,
    glm::uvec3 offset,
    glm::uvec3 extent,
    VkDeviceMemory memory,
    uint64_t memory_offset)
{
    const VkImageAspectFlags aspect_mask =
        is_depth_format(image.get_format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkSparseImageMemoryBind bind{};
    bind.subresource = VkImageSubresource{
        .aspectMask = aspect_mask,
        .mipLevel = mip_level,
        .arrayLayer = array_layer,
    };
    bind.offset = VkOffset3D{
        .x = static_cast<int32_t>(offset.x),
        .y = static_cast<int32_t>(offset.y),
        .z = static_cast<int32_t>(offset.z),
    };
    bind.extent = VkExtent3D{
        .width = extent.x,
        .height = extent.y,
        .depth = extent.z,
    };
    bind.memory = memory;
    bind.memoryOffset = memory_offset;
    bind.flags = 0;

    return bind;
}

VkSparseMemoryBind to_vulkan_bind(const SparseBufferBind& bind)
{
    return get_vulkan_sparse_memory_bind(
        bind.resource_offset, bind.size, get_vulkan_heap_memory(bind.heap), bind.heap_offset);
}

VkSparseMemoryBind to_vulkan_bind(const SparseBufferUnbind& unbind)
{
    return get_vulkan_sparse_memory_bind(unbind.resource_offset, unbind.size, VK_NULL_HANDLE, 0);
}

VkSparseImageMemoryBind to_vulkan_bind(const SparseImageBind& bind)
{
    return get_vulkan_sparse_image_memory_bind(
        *bind.image,
        bind.mip_level,
        bind.array_layer,
        bind.offset,
        bind.extent,
        get_vulkan_heap_memory(bind.heap),
        bind.heap_offset);
}

VkSparseImageMemoryBind to_vulkan_bind(const SparseImageUnbind& unbind)
{
    return get_vulkan_sparse_image_memory_bind(
        *unbind.image, unbind.mip_level, unbind.array_layer, unbind.offset, unbind.extent, VK_NULL_HANDLE, 0);
}

VkSparseMemoryBind to_vulkan_bind(const SparseMipTailBind& bind)
{
    const VulkanImageResource& native_image = static_cast<const VulkanImageResource&>(*bind.image);

    return get_vulkan_sparse_memory_bind(
        native_image.get_mip_tail_resource_offset(bind.array_layer),
        native_image.get_sparse_memory_requirements().mip_tail_size,
        get_vulkan_heap_memory(bind.heap),
        bind.heap_offset);
}

VkSparseMemoryBind to_vulkan_bind(const SparseMipTailUnbind& unbind)
{
    const VulkanImageResource& native_image = static_cast<const VulkanImageResource&>(*unbind.image);

    return get_vulkan_sparse_memory_bind(
        native_image.get_mip_tail_resource_offset(unbind.array_layer),
        native_image.get_sparse_memory_requirements().mip_tail_size,
        VK_NULL_HANDLE,
        0);
}

VkDeviceMemory get_vulkan_metadata_memory(const SparseMetadataBind& bind)
{
    return get_vulkan_heap_memory(bind.heap);
}

VkDeviceMemory get_vulkan_metadata_memory(const SparseMetadataUnbind&)
{
    return VK_NULL_HANDLE;
}

uint64_t get_vulkan_metadata_heap_offset(const SparseMetadataBind& bind)
{
    return bind.heap_offset;
}

uint64_t get_vulkan_metadata_heap_offset(const SparseMetadataUnbind&)
{
    return 0;
}

struct VulkanSparseBindSync
{
    std::vector<VkSemaphore> wait_semaphores{};
    std::vector<uint64_t> wait_values{};
    std::vector<VkSemaphore> signal_semaphores{};
    std::vector<uint64_t> signal_values{};

    VkTimelineSemaphoreSubmitInfo timeline_info{};

    void add_wait(VkSemaphore semaphore, uint64_t value)
    {
        wait_semaphores.push_back(semaphore);
        wait_values.push_back(value);
    }

    void add_signal(VkSemaphore semaphore, uint64_t value)
    {
        signal_semaphores.push_back(semaphore);
        signal_values.push_back(value);
    }

    VkBindSparseInfo build()
    {
        timeline_info = VkTimelineSemaphoreSubmitInfo{};
        timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timeline_info.waitSemaphoreValueCount = static_cast<uint32_t>(wait_values.size());
        timeline_info.pWaitSemaphoreValues = wait_values.data();
        timeline_info.signalSemaphoreValueCount = static_cast<uint32_t>(signal_values.size());
        timeline_info.pSignalSemaphoreValues = signal_values.data();

        VkBindSparseInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        info.pNext = &timeline_info;
        info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
        info.pWaitSemaphores = wait_semaphores.data();
        info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
        info.pSignalSemaphores = signal_semaphores.data();

        return info;
    }
};

} // namespace

SyncPoint VulkanQueue::bind_sparse(const SparseBindBatch& batch, const SubmitInfo& info)
{
    MIZU_PROFILE_SCOPED;

    size_t num_metadata_regions = 0;
    const auto count_metadata_regions = [&](const auto& op) {
        num_metadata_regions += static_cast<const VulkanImageResource&>(*op.image).get_metadata_region_count();
    };
    std::ranges::for_each(batch.get_metadata_binds(), count_metadata_regions);
    std::ranges::for_each(batch.get_metadata_unbinds(), count_metadata_regions);

    const size_t num_buffer_ops = batch.get_buffer_binds().size() + batch.get_buffer_unbinds().size();
    const size_t num_image_ops = batch.get_image_binds().size() + batch.get_image_unbinds().size();
    const size_t num_mip_tail_ops = batch.get_mip_tail_binds().size() + batch.get_mip_tail_unbinds().size();
    const size_t num_metadata_ops = batch.get_metadata_binds().size() + batch.get_metadata_unbinds().size();

    // TODO: Temporary allocations :(
    std::vector<VkSparseMemoryBind> buffer_binds;
    std::vector<VkSparseMemoryBind> opaque_binds;
    std::vector<VkSparseImageMemoryBind> image_binds;
    buffer_binds.reserve(num_buffer_ops);
    opaque_binds.reserve(num_mip_tail_ops + num_metadata_regions);
    image_binds.reserve(num_image_ops);

    std::vector<VkSparseBufferMemoryBindInfo> buffer_infos;
    std::vector<VkSparseImageOpaqueMemoryBindInfo> opaque_infos;
    std::vector<VkSparseImageMemoryBindInfo> image_infos;
    buffer_infos.reserve(num_buffer_ops);
    opaque_infos.reserve(num_mip_tail_ops + num_metadata_ops);
    image_infos.reserve(num_image_ops);

    const auto add_buffer_op = [&](const auto& op) {
        const VkSparseMemoryBind& bind = buffer_binds.emplace_back(to_vulkan_bind(op));
        buffer_infos.push_back(
            VkSparseBufferMemoryBindInfo{
                .buffer = static_cast<const VulkanBufferResource&>(*op.buffer).handle(),
                .bindCount = 1,
                .pBinds = &bind,
            });
    };

    const auto add_image_op = [&](const auto& op) {
        const VkSparseImageMemoryBind& bind = image_binds.emplace_back(to_vulkan_bind(op));
        image_infos.push_back(
            VkSparseImageMemoryBindInfo{
                .image = static_cast<const VulkanImageResource&>(*op.image).handle(),
                .bindCount = 1,
                .pBinds = &bind,
            });
    };

    const auto add_mip_tail_op = [&](const auto& op) {
        const VkSparseMemoryBind& bind = opaque_binds.emplace_back(to_vulkan_bind(op));
        opaque_infos.push_back(
            VkSparseImageOpaqueMemoryBindInfo{
                .image = static_cast<const VulkanImageResource&>(*op.image).handle(),
                .bindCount = 1,
                .pBinds = &bind,
            });
    };

    const auto add_metadata_op = [&](const auto& op) {
        const VulkanImageResource& native_image = static_cast<const VulkanImageResource&>(*op.image);

        const VkDeviceMemory memory = get_vulkan_metadata_memory(op);
        const uint64_t heap_offset = get_vulkan_metadata_heap_offset(op);
        const uint64_t region_size = native_image.get_metadata_region_size();
        const uint32_t region_count = native_image.get_metadata_region_count();

        const size_t first = opaque_binds.size();
        for (uint32_t region = 0; region < region_count; ++region)
        {
            opaque_binds.push_back(get_vulkan_sparse_memory_bind(
                native_image.get_metadata_resource_offset(region),
                region_size,
                memory,
                memory == VK_NULL_HANDLE ? 0 : heap_offset + region * region_size,
                VK_SPARSE_MEMORY_BIND_METADATA_BIT));
        }

        opaque_infos.push_back(
            VkSparseImageOpaqueMemoryBindInfo{
                .image = native_image.handle(),
                .bindCount = region_count,
                .pBinds = opaque_binds.data() + first,
            });
    };

    std::ranges::for_each(batch.get_buffer_unbinds(), add_buffer_op);
    std::ranges::for_each(batch.get_buffer_binds(), add_buffer_op);
    std::ranges::for_each(batch.get_image_unbinds(), add_image_op);
    std::ranges::for_each(batch.get_image_binds(), add_image_op);
    std::ranges::for_each(batch.get_mip_tail_unbinds(), add_mip_tail_op);
    std::ranges::for_each(batch.get_mip_tail_binds(), add_mip_tail_op);
    std::ranges::for_each(batch.get_metadata_unbinds(), add_metadata_op);
    std::ranges::for_each(batch.get_metadata_binds(), add_metadata_op);

    const std::lock_guard lock(m_mutex);

    VulkanSparseBindSync sync{};

    for (const SyncPoint& wait : info.waits)
    {
        if (wait.is_valid())
            m_pending_waits.push_back(vulkan_semaphore_submit_info(wait));
    }

    for (const VkSemaphoreSubmitInfo& wait : m_pending_waits)
        sync.add_wait(wait.semaphore, wait.value);

    m_last_value += 1;
    sync.add_signal(m_fence->handle(), m_last_value);

    for (const SyncPoint& signal : info.signals)
        sync.add_signal(static_cast<const VulkanFence*>(signal.fence)->handle(), signal.value);

    VkBindSparseInfo bind_info = sync.build();
    bind_info.bufferBindCount = static_cast<uint32_t>(buffer_infos.size());
    bind_info.pBufferBinds = buffer_infos.data();
    bind_info.imageOpaqueBindCount = static_cast<uint32_t>(opaque_infos.size());
    bind_info.pImageOpaqueBinds = opaque_infos.data();
    bind_info.imageBindCount = static_cast<uint32_t>(image_infos.size());
    bind_info.pImageBinds = image_infos.data();

    VK_CHECK(vkQueueBindSparse(m_handle, 1, &bind_info, VK_NULL_HANDLE));

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
