#pragma once

#include <cstdint>

#include "render_core/definitions/resource.h"
#include "render_core/rhi/command_buffer.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

VkSharingMode get_vulkan_sharing_mode(ResourceSharingMode mode);

using QueueFamiliesArray = inplace_vector<uint32_t, enum_metadata_count_v<CommandBufferType>>;
void get_queue_families_array(typed_bitset<CommandBufferType> bitset, QueueFamiliesArray& out_queue_families);

} // namespace Mizu::Vulkan