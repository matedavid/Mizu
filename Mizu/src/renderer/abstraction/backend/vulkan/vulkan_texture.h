#pragma once

#include "renderer/abstraction/texture.h"

#include <vulkan/vulkan.h>

namespace Mizu::Vulkan {

// Forward declarations
class VulkanImage;

class VulkanTexture2D : public Texture2D {
  public:
    explicit VulkanTexture2D(const ImageDescription& desc);
    ~VulkanTexture2D() override;

    [[nodiscard]] ImageFormat get_format() const override { return m_description.format; }
    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }

    [[nodiscard]] std::shared_ptr<VulkanImage> get_image() const { return m_image; }
    [[nodiscard]] VkSampler get_sampler() const { return m_sampler; }

  private:
    std::shared_ptr<VulkanImage> m_image;
    VkSampler m_sampler{VK_NULL_HANDLE};

    ImageDescription m_description;

    [[nodiscard]] static VkFilter get_filter(ImageFilter filter);
    [[nodiscard]] static VkSamplerAddressMode get_sampler_address_mode(ImageAddressMode mode);
};

} // namespace Mizu::Vulkan
