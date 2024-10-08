#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/backend/vulkan/vulkan_image.h"
#include "renderer/abstraction/cubemap.h"

namespace Mizu::Vulkan {

class VulkanCubemap : public VulkanImage, public Cubemap {
  public:
    VulkanCubemap(const ImageDescription& desc);
    VulkanCubemap(const Faces& faces);

  private:
    static void load_face(const std::filesystem::path& path,
                          std::vector<uint8_t>& data,
                          uint32_t& width,
                          uint32_t& height,
                          uint32_t idx);
};

} // namespace Mizu::Vulkan