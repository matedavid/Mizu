#pragma once

#include <memory>

#include "backend/vulkan/vulkan_device.h"
#include "backend/vulkan/vulkan_instance.h"

namespace Mizu::Vulkan {

struct VulkanContextT {
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanDevice> device;
};

extern VulkanContextT VulkanContext;

} // namespace Mizu::Vulkan