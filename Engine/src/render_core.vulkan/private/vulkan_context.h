#pragma once

#include <memory>

#include "vulkan_core.h"
#include "vulkan_debug.h"
#include "vulkan_descriptors.h"
#include "vulkan_descriptors2.h"
#include "vulkan_device.h"
#include "vulkan_device_memory_allocator.h"
#include "vulkan_pipeline_layout.h"

namespace Mizu::Vulkan
{

struct VulkanContextT
{
    ~VulkanContextT();

    VulkanDevice* device;
    std::unique_ptr<VulkanPipelineLayoutCache> pipeline_layout_cache;
    std::unique_ptr<VulkanDescriptorLayoutCache> layout_cache;
    std::unique_ptr<VulkanDescriptorPool> descriptor_pool;
    std::unique_ptr<IDeviceMemoryAllocator> default_device_allocator;
    std::unique_ptr<VulkanDescriptorManager> descriptor_manager;

    VulkanBindingOffsets binding_offsets;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtx_properties;
};

extern VulkanContextT VulkanContext;

// Function pointers
extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;

extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

void initialize_rtx(VkDevice device, VkPhysicalDevice physical_device);

VkDeviceAddress get_device_address(VkBuffer buffer);
VkDeviceAddress get_device_address(VkAccelerationStructureKHR as);

} // namespace Mizu::Vulkan
