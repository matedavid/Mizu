#include "vulkan_device_memory_allocator.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image_resource.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

VulkanBaseDeviceMemoryAllocator::~VulkanBaseDeviceMemoryAllocator() {
    for (const auto& alloc : m_allocations) {
        vkFreeMemory(VulkanContext.device->handle(), alloc.second, nullptr);
    }
}

std::shared_ptr<ImageResource> VulkanBaseDeviceMemoryAllocator::allocate_image_resource(
    const ImageDescription& desc,
    const SamplingOptions& sampling) {
    auto resource = std::make_shared<VulkanImageResource>(desc, sampling, false);

    resource->create_image();
    allocate_image(resource.get());

    resource->create_image_views();
    resource->create_sampler();

    return resource;
}

std::shared_ptr<ImageResource> VulkanBaseDeviceMemoryAllocator::allocate_image_resource(const ImageDescription& desc,
                                                                                        const SamplingOptions& sampling,
                                                                                        const uint8_t* data,
                                                                                        uint32_t size) {
    const auto resource = allocate_image_resource(desc, sampling);

    // Transition image layout for copying
    VulkanRenderCommandBuffer::submit_single_time(
        [&](const VulkanCommandBufferBase<CommandBufferType::Graphics>& command_buffer) {
            command_buffer.transition_resource(
                *resource, ImageResourceState::Undefined, ImageResourceState::TransferDst);
        });

    // Create staging buffer
    const auto staging_buffer = VulkanBuffer{
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    staging_buffer.copy_data(data);

    // Copy staging buffer to image
    staging_buffer.copy_to_image(*std::dynamic_pointer_cast<VulkanImageResource>(resource));

    // Transition image layout for shader access
    VulkanRenderCommandBuffer::submit_single_time(
        [&](const VulkanCommandBufferBase<CommandBufferType::Graphics>& command_buffer) {
            command_buffer.transition_resource(
                *resource, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);
        });

    return resource;
}

void VulkanBaseDeviceMemoryAllocator::release(const std::shared_ptr<ImageResource>& resource) {
    const auto& native_reosurce = std::dynamic_pointer_cast<VulkanImageResource>(resource);

    const size_t id = std::hash<const void*>()(native_reosurce.get()); // TODO: UGLY

    const auto it = m_allocations.find(id);
    if (it == m_allocations.end()) {
        MIZU_LOG_ERROR("ImageResource has not been allocated by this allocator");
        return;
    }

    // Destroy image
    vkDestroySampler(VulkanContext.device->handle(), native_reosurce->get_sampler(), nullptr);
    vkDestroyImageView(VulkanContext.device->handle(), native_reosurce->get_image_view(), nullptr);
    vkDestroyImage(VulkanContext.device->handle(), native_reosurce->get_image_handle(), nullptr);

    // Fre memory
    vkFreeMemory(VulkanContext.device->handle(), it->second, nullptr);

    m_allocations.erase(it);
}

void VulkanBaseDeviceMemoryAllocator::allocate_image(const VulkanImageResource* image) {
    const size_t id = std::hash<const void*>()(image); // TODO: UGLY

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(VulkanContext.device->handle(), image->get_image_handle(), &memory_requirements);

    const std::optional<uint32_t> memory_type_index =
        VulkanContext.device->find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    MIZU_ASSERT(memory_type_index.has_value(), "No suitable memory to allocate image");

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = *memory_type_index;

    VkDeviceMemory memory;

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &memory));
    VK_CHECK(vkBindImageMemory(VulkanContext.device->handle(), image->get_image_handle(), memory, 0));

    m_allocations.insert({id, memory});
}

} // namespace Mizu::Vulkan
