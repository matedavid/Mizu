#pragma once

#include <vulkan/vulkan.h>

#include "texture.h"

namespace Mizu::Vulkan {

class VulkanImage {
  public:
    struct Description {
        uint32_t width = 1, height = 1, depth = 1;
        ImageFormat format = ImageFormat::RGBA8_SRGB;
        VkImageType type = VK_IMAGE_TYPE_2D;
        uint32_t num_layers = 1;
        uint32_t mip_levels = 1;

        // Usage options
        bool sampled = true;
        bool storage = false;
        bool cubemap = false;
    };

    explicit VulkanImage(const Description& desc);
    ~VulkanImage();

    [[nodiscard]] static VkFormat get_image_format(ImageFormat format);
    [[nodiscard]] static VkImageViewType get_image_view_type(VkImageType type);

    [[nodiscard]] VkImage get_image_handle() const { return m_image; }
    [[nodiscard]] VkImageView get_image_view() const { return m_image_view; }

  private:
    VkImage m_image{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    VkImageView m_image_view{VK_NULL_HANDLE};

    Description m_description;

    void create_image_view();
};

} // namespace Mizu::Vulkan
