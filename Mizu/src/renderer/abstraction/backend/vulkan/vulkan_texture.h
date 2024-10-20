#pragma once

#include <vulkan/vulkan.h>

#include "renderer/abstraction/backend/vulkan/vulkan_image.h"
#include "renderer/abstraction/texture.h"

namespace Mizu::Vulkan {

class VulkanTexture2D : public VulkanImage, public Texture2D {
  public:
    explicit VulkanTexture2D(const ImageDescription& desc);
    VulkanTexture2D(const std::filesystem::path& path, SamplingOptions sampling);
    VulkanTexture2D(uint32_t width, uint32_t height, VkImage image, VkImageView view, bool owning = false);
};

} // namespace Mizu::Vulkan
