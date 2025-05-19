#pragma once

#include "render_core/rhi/backend/vulkan/vk_core.h"

namespace Mizu::Vulkan
{

extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;

void initialize_rtx(VkDevice device);

} // namespace Mizu::Vulkan