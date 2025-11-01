#pragma once

#include <cstdint>

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"

namespace Mizu
{

class IRHIWindow
{
  public:
    virtual ~IRHIWindow() = default;

    virtual VkResult create_vulkan_surface(const VkInstance& instance, VkSurfaceKHR& surface) const = 0;
    virtual HWND create_dx12_window_handle() const = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu