#include "opengl_device_memory_allocator.h"

#include "utility/assert.h"

namespace Mizu::OpenGL {

Allocation OpenGLBaseDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image) {
    MIZU_UNREACHABLE("OpenGL does not support manual memory allocation");
    return Allocation::invalid();
}

void OpenGLBaseDeviceMemoryAllocator::release([[maybe_unused]] Allocation id) {}

} // namespace Mizu::OpenGL