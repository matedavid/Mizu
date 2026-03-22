#pragma once

#include <cstdint>

#include "base/containers/inplace_vector.h"
#include "base/containers/typed_bitset.h"
#include "render_core/definitions/resource.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/render_pass2.h"
#include "render_core/rhi/image_resource.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

VkBufferUsageFlags get_vulkan_buffer_usage(BufferUsageBits usage);

VkImageType get_vulkan_image_type(ImageType type);
VkFormat get_vulkan_image_format(ImageFormat format);
VkImageLayout get_vulkan_image_resource_state(ImageResourceState state);
VkImageUsageFlags get_vulkan_image_usage(ImageUsageBits usage, ImageFormat format);

VkSharingMode get_vulkan_sharing_mode(ResourceSharingMode mode);

using QueueFamiliesArray = inplace_vector<uint32_t, enum_metadata_count_v<CommandBufferType>>;
void get_vulkan_queue_families_array(typed_bitset<CommandBufferType> bitset, QueueFamiliesArray& out_queue_families);

VkAttachmentLoadOp get_vulkan_load_operation(LoadOperation op);
VkAttachmentStoreOp get_vulkan_store_operation(StoreOperation op);

} // namespace Mizu::Vulkan
