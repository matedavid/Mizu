#pragma once

#include <memory>

#include "renderer/abstraction/backend/vulkan/vulkan_descriptors.h"
#include "renderer/abstraction/backend/vulkan/vulkan_device.h"
#include "renderer/abstraction/backend/vulkan/vulkan_instance.h"

namespace Mizu::Vulkan {

struct VulkanContextT {
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanDevice> device;
    std::unique_ptr<VulkanDescriptorLayoutCache> layout_cache;
};

extern VulkanContextT VulkanContext;

} // namespace Mizu::Vulkan
