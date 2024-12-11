#pragma once

#include <vulkan/vulkan.h>

namespace Mizu::Vulkan
{

class VulkanUtils
{
  public:
    static uint32_t get_format_size(VkFormat format);
};

} // namespace Mizu::Vulkan
