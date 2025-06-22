#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace Mizu
{

class IRHIWindow
{
  public:
    virtual ~IRHIWindow() = default;

    virtual VkResult create_vulkan_surface(const VkInstance& instance, VkSurfaceKHR& surface) const = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu