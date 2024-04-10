#pragma once

#include <memory>

namespace Mizu::Vulkan {

// Forward declarations
class VulkanInstance;

struct VulkanContextT {
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
};

extern VulkanContextT VulkanContext;

} // namespace Mizu::Vulkan