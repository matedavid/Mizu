#pragma once

#include "renderer/abstraction/device_memory_allocator.h"

namespace Mizu::OpenGL {

class OpenGLBaseDeviceMemoryAllocator : public BaseDeviceMemoryAllocator {
  public:
    OpenGLBaseDeviceMemoryAllocator() = default;

    std::shared_ptr<ImageResource> allocate_image_resource(const ImageDescription& desc,
                                                           const SamplingOptions& sampling) override;
    std::shared_ptr<ImageResource> allocate_image_resource(const ImageDescription& desc,
                                                           const SamplingOptions& sampling,
                                                           const uint8_t* data,
                                                           uint32_t size) override;

    void release(const std::shared_ptr<ImageResource>& resource) override;
};

} // namespace Mizu::OpenGL