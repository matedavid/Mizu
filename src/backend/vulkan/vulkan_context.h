#pragma once

#include <memory>

namespace Mizu::Vulkan {

// Forward declarations
class VulkanInstance;

struct VulkanContextT {
    VulkanContextT() = default;
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
};

static VulkanContextT VulkanContext;

} // namespace Mizu