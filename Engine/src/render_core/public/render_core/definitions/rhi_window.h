#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

#include "mizu_render_core_module.h"

namespace Mizu
{

class MIZU_RENDER_CORE_API IRHIWindow
{
  public:
    virtual ~IRHIWindow() = default;

    virtual VkResult create_vulkan_surface(VkInstance_T* instance, VkSurfaceKHR_T*& surface) const = 0;
    virtual void* create_dx12_window_handle() const = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu