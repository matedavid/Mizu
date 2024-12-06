#include "opengl_device_memory_allocator.h"

#include "renderer/abstraction/renderer.h"

#include "utility/assert.h"

namespace Mizu::OpenGL {

Allocation OpenGLBaseDeviceMemoryAllocator::allocate_buffer_resource(const BufferResource& buffer) {
    MIZU_UNREACHABLE("OpenGL does not support manual memory allocation");
    return Allocation::invalid();
}

Allocation OpenGLBaseDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image) {
    MIZU_UNREACHABLE("OpenGL does not support manual memory allocation");
    return Allocation::invalid();
}

void OpenGLBaseDeviceMemoryAllocator::release([[maybe_unused]] Allocation id) {
    MIZU_UNREACHABLE("OpenGL does not support manual memory allocation");
}

//
//
//

OpenGLTransientImageResource::OpenGLTransientImageResource(const ImageDescription& desc,
                                                           const SamplingOptions& sampling) {
    m_resource = std::make_shared<OpenGLImageResource>(desc, sampling);
}

OpenGLTransientBufferResource::OpenGLTransientBufferResource(const BufferDescription& desc) {
    m_resource = std::make_unique<OpenGLBufferResource>(desc);
}

void OpenGLRenderGraphDeviceMemoryAllocator::allocate_image_resource(
    [[maybe_unused]] const TransientImageResource& resource,
    [[maybe_unused]] size_t offset) {}

void OpenGLRenderGraphDeviceMemoryAllocator::allocate_buffer_resource(
    [[maybe_unused]] const TransientBufferResource& resource,
    [[maybse_unused]] size_t offset) {}

void OpenGLRenderGraphDeviceMemoryAllocator::allocate() {}

} // namespace Mizu::OpenGL