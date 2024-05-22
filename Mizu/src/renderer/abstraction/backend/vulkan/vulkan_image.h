#pragma once

#include <vulkan/vulkan.h>

#include "renderer/abstraction/texture.h"

namespace Mizu::Vulkan {

class VulkanImage {
  public:
    struct Description {
        uint32_t width = 1, height = 1, depth = 1;
        ImageFormat format = ImageFormat::RGBA8_SRGB;
        VkImageType type = VK_IMAGE_TYPE_2D;
        VkImageUsageFlags usage = 0;
        VkImageCreateFlags flags = 0;

        uint32_t num_layers = 1;
        uint32_t mip_levels = 1;
    };

    explicit VulkanImage(const Description& desc);
    VulkanImage(VkImage image, VkImageView view, bool owning = false);
    ~VulkanImage();

    [[nodiscard]] static VkFormat get_image_format(ImageFormat format);
    [[nodiscard]] static VkImageViewType get_image_view_type(VkImageType type);

    [[nodiscard]] VkImage get_image_handle() const { return m_image; }
    [[nodiscard]] VkImageView get_image_view() const { return m_image_view; }

    [[nodiscard]] uint32_t get_width() const { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const { return m_description.height; }
    [[nodiscard]] uint32_t get_num_layers() const { return m_description.num_layers; }
    [[nodiscard]] uint32_t get_mip_levels() const { return m_description.mip_levels; }

  private:
    VkImage m_image{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    VkImageView m_image_view{VK_NULL_HANDLE};

    bool m_owns_resources = true;

    Description m_description;

    void create_image_view();
};

} // namespace Mizu::Vulkan
