#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/device_memory_allocator.h"

namespace Mizu::Vulkan {

class VulkanBaseDeviceMemoryAllocator : public BaseDeviceMemoryAllocator {
  public:
    VulkanBaseDeviceMemoryAllocator() = default;
    ~VulkanBaseDeviceMemoryAllocator() override;

    Allocation allocate_image_resource(const ImageResource& image) override;

    void release(Allocation id) override;

  private:
    std::unordered_map<Allocation, VkDeviceMemory> m_memory_allocations;
};

} // namespace Mizu::Vulkan