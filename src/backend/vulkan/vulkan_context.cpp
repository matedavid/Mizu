#include "vulkan_context.h"

#include "backend/vulkan/vulkan_instance.h"
#include "backend/vulkan/vulkan_device.h"

namespace Mizu::Vulkan {

VulkanContextT VulkanContext{};

VulkanContextT::~VulkanContextT() = default;

} // namespace Mizu::Vulkan