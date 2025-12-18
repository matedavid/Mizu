#pragma once

#include <cstdint>

#if MIZU_RENDER_CORE_VULKAN_ENABLED
#include <vulkan/vulkan.h>
#endif

#include "mizu_render_core_module.h"

namespace Mizu
{

class IRHIWindow
{
  public:
    virtual ~IRHIWindow() = default;

#if MIZU_RENDER_CORE_VULKAN_ENABLED
    virtual VkResult create_vulkan_surface(VkInstance_T* instance, VkSurfaceKHR_T*& surface) const = 0;
#endif
#if MIZU_RENDER_CORE_DX12_ENABLED
    virtual void* create_dx12_window_handle() const = 0;
#endif

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu