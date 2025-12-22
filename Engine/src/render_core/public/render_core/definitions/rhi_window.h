#pragma once

#include <cstdint>

#include "mizu_render_core_module.h"

#if MIZU_RENDER_CORE_VULKAN_ENABLED
struct VkInstance_T;
struct VkSurfaceKHR_T;
#endif

namespace Mizu
{

class IRHIWindow
{
  public:
    virtual ~IRHIWindow() = default;

#if MIZU_RENDER_CORE_VULKAN_ENABLED
    virtual void create_vulkan_surface(VkInstance_T* instance, VkSurfaceKHR_T*& surface) const = 0;
#endif
#if MIZU_RENDER_CORE_DX12_ENABLED
    virtual void* create_dx12_window_handle() const = 0;
#endif

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu