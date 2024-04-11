#pragma once

#include <memory>

namespace Mizu::Vulkan {

// Forward declarations
class VulkanInstance;
class VulkanDevice;

struct VulkanContextT {
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanDevice> device;
};

extern VulkanContextT VulkanContext;

} // namespace Mizu::Vulkan