#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "base/types/uuid.h"

#include "render_core/rhi/device_memory.h"

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

class IDeviceMemoryAllocator
{
  public:
    virtual ~IDeviceMemoryAllocator() = default;

    virtual AllocationInfo allocate_buffer_resource(const BufferResource& buffer) = 0;
    virtual AllocationInfo allocate_image_resource(const ImageResource& image) = 0;

    virtual uint8_t* get_mapped_memory(AllocationId id) const = 0;

    virtual void release(AllocationId id) = 0;
    virtual void reset() = 0;
};

class BaseDeviceMemoryAllocator : public IDeviceMemoryAllocator
{
  public:
    static std::shared_ptr<BaseDeviceMemoryAllocator> create();
};

class AliasedDeviceMemoryAllocator
{
  public:
    virtual ~AliasedDeviceMemoryAllocator() = default;

    static std::shared_ptr<AliasedDeviceMemoryAllocator> create(bool host_visible = false, std::string name = "");

    virtual void allocate_buffer_resource(const BufferResource& buffer, size_t offset) = 0;
    virtual void allocate_image_resource(const ImageResource& image, size_t offset) = 0;

    virtual uint8_t* get_mapped_memory() const = 0;

    virtual void allocate() = 0;

    virtual size_t get_allocated_size() const = 0;
};

} // namespace Mizu