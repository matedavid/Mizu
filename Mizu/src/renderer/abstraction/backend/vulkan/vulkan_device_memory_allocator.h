#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/device_memory_allocator.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanImageResource;

class VulkanBaseDeviceMemoryAllocator : public BaseDeviceMemoryAllocator {
  public:
    VulkanBaseDeviceMemoryAllocator() = default;
    ~VulkanBaseDeviceMemoryAllocator() override;

    std::shared_ptr<ImageResource> allocate_image_resource(const ImageDescription& desc,
                                                           const SamplingOptions& sampling) override;
    std::shared_ptr<ImageResource> allocate_image_resource(const ImageDescription& desc,
                                                           const SamplingOptions& sampling,
                                                           const uint8_t* data,
                                                           uint32_t size) override;

    void release(const std::shared_ptr<ImageResource>& resource) override;

  private:
    void allocate_image(const VulkanImageResource* image);

    std::unordered_map<size_t, VkDeviceMemory> m_allocations;
};

} // namespace Mizu::Vulkan