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
    AllocationId id;
    size_t size;
    size_t offset;
    DeviceMemory device_memory;
};

class AliasedDeviceMemoryAllocator
{
  public:
    virtual ~AliasedDeviceMemoryAllocator() = default;

    virtual void allocate_buffer_resource(const BufferResource& buffer, size_t offset) = 0;
    virtual void allocate_image_resource(const ImageResource& image, size_t offset) = 0;

    virtual uint8_t* get_mapped_memory() const = 0;

    virtual void allocate() = 0;

    virtual size_t get_allocated_size() const = 0;
};

class TransientMemoryPool
{
  public:
    virtual ~TransientMemoryPool() = default;

    virtual void place_buffer(const BufferResource& buffer, size_t offset) = 0;
    virtual void place_image(const ImageResource& image, size_t offset) = 0;

    virtual void commit() = 0;
    virtual void reset() = 0;

    virtual size_t get_committed_size() const = 0;
};

} // namespace Mizu