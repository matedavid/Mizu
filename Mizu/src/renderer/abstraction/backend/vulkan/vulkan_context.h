#pragma once

#include <memory>

#include "backend/vulkan/vulkan_device.h"
#include "backend/vulkan/vulkan_instance.h"
#include "backend/vulkan/vulkan_descriptors.h"

namespace Mizu::Vulkan {

struct VulkanContextT {
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanDevice> device;
    std::unique_ptr<VulkanDescriptorLayoutCache> layout_cache;
};

extern VulkanContextT VulkanContext;

} // namespace Mizu::Vulkan