#pragma once

#include <vulkan/vulkan.h>

#include "render_core/rhi/sampler_state.h"

namespace Mizu::Vulkan
{

class VulkanSamplerState : public SamplerState
{
  public:
    VulkanSamplerState(SamplingOptions options);
    ~VulkanSamplerState() override;

    static VkFilter get_vulkan_filter(ImageFilter filter);
    static VkSamplerAddressMode get_vulkan_sampler_address_mode(ImageAddressMode mode);
    static VkBorderColor get_vulkan_border_color(BorderColor color);

    VkSampler handle() const { return m_sampler; }

  private:
    VkSampler m_sampler{VK_NULL_HANDLE};
};

} // namespace Mizu::Vulkan