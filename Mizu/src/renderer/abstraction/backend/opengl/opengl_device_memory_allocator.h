#pragma once

#include "renderer/abstraction/device_memory_allocator.h"

namespace Mizu::OpenGL {

class OpenGLBaseDeviceMemoryAllocator : public BaseDeviceMemoryAllocator {
  public:
    OpenGLBaseDeviceMemoryAllocator() = default;
    ~OpenGLBaseDeviceMemoryAllocator() override = default;

    Allocation allocate_image_resource(const ImageResource& image) override;

    void release(Allocation id) override;
};

} // namespace Mizu::OpenGL