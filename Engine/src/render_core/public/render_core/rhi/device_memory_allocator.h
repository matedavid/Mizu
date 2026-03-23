#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "base/types/uuid.h"

#include "mizu_render_core_module.h"
#include "render_core/definitions/device_memory.h"

namespace Mizu
{

// Forward declarations
class BufferResource;
class ImageResource;

using AllocationId = UUID;
using DeviceMemory = void*;

struct AllocationInfo
{
    AllocationId id = AllocationId(0);
    size_t size = 0;
    size_t offset = 0;
    DeviceMemory device_memory = nullptr;
};

class TransientMemoryPool
{
  public:
    virtual ~TransientMemoryPool() = default;

    virtual void place_buffer(BufferResource& buffer, size_t offset) = 0;
    virtual void place_image(ImageResource& image, size_t offset) = 0;

    virtual void commit() = 0;
    virtual void reset() = 0;

    virtual size_t get_committed_size() const = 0;
};

} // namespace Mizu