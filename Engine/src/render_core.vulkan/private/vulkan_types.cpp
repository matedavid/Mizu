#include "vulkan_types.h"

#include "vulkan_context.h"

namespace Mizu::Vulkan
{

VkSharingMode get_vulkan_sharing_mode(ResourceSharingMode mode)
{
    switch (mode)
    {
    case ResourceSharingMode::Exclusive:
        return VK_SHARING_MODE_EXCLUSIVE;
    case ResourceSharingMode::Concurrent:
        return VK_SHARING_MODE_CONCURRENT;
    }
}

void get_queue_families_array(typed_bitset<CommandBufferType> bitset, QueueFamiliesArray& out_queue_families)
{
    static constexpr std::array queue_families = {
        CommandBufferType::Graphics, CommandBufferType::Compute, CommandBufferType::Transfer};

    for (CommandBufferType type : queue_families)
    {
        if (bitset.test(type) && VulkanContext.device->is_queue_available(type))
        {
            out_queue_families.push_back(VulkanContext.device->get_queue(type)->family());
        }
        else if (bitset.test(type) && !VulkanContext.device->is_queue_available(type))
        {
            MIZU_LOG_WARNING("Trying to use a queue family that is not available, ignoring");
        }
    }
}

VkAttachmentLoadOp get_load_operation(LoadOperation op)
{
    switch (op)
    {
    case LoadOperation::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOperation::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp get_store_operation(StoreOperation op)
{
    switch (op)
    {
    case StoreOperation::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case StoreOperation::DontCare:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::None:
        return VK_ATTACHMENT_STORE_OP_NONE;
    }
}

} // namespace Mizu::Vulkan
