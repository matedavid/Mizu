#include "vulkan_rtx_core.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"

namespace Mizu::Vulkan
{

PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;

PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

void initialize_rtx(VkDevice device)
{
    vkGetBufferDeviceAddressKHR =
        (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
    vkCreateAccelerationStructureKHR =
        (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    vkDestroyAccelerationStructureKHR =
        (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    vkCmdBuildAccelerationStructuresKHR =
        (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(
        device, "vkGetAccelerationStructureBuildSizesKHR");

    vkCreateRayTracingPipelinesKHR =
        (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
}

} // namespace Mizu::Vulkan