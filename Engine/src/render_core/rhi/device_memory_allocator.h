#pragma once

#include <cstdint>
#include <memory>

#include "base/debug/assert.h"
#include "base/types/uuid.h"

namespace Mizu
{

// Forward declarations
class BufferResource;
class ImageResource;

using Allocation = UUID;

class IDeviceMemoryAllocator
{
  public:
    virtual ~IDeviceMemoryAllocator() = default;

    virtual Allocation allocate_buffer_resource(const BufferResource& buffer) = 0;
    virtual Allocation allocate_image_resource(const ImageResource& image) = 0;

    virtual void release(Allocation id) = 0;
};

class BaseDeviceMemoryAllocator : public IDeviceMemoryAllocator
{
  public:
    static std::shared_ptr<BaseDeviceMemoryAllocator> create();
};

//
// RenderGraphDeviceMemoryAllocator
//

struct ImageDescription;
struct BufferDescription;

class TransientImageResource
{
  public:
    virtual ~TransientImageResource() = default;

    static std::shared_ptr<TransientImageResource> create(const ImageDescription& desc);

    [[nodiscard]] virtual uint64_t get_size() const = 0;
    [[nodiscard]] virtual uint64_t get_alignment() const = 0;

    [[nodiscard]] virtual std::shared_ptr<ImageResource> get_resource() const = 0;
};

class TransientBufferResource
{
  public:
    virtual ~TransientBufferResource() = default;

    static std::shared_ptr<TransientBufferResource> create(const BufferDescription& desc);

    [[nodiscard]] virtual uint64_t get_size() const = 0;
    [[nodiscard]] virtual uint64_t get_alignment() const = 0;

    [[nodiscard]] virtual std::shared_ptr<BufferResource> get_resource() const = 0;
};

class RenderGraphDeviceMemoryAllocator
{
  public:
    virtual ~RenderGraphDeviceMemoryAllocator() = default;

    static std::shared_ptr<RenderGraphDeviceMemoryAllocator> create();

    virtual void allocate_image_resource(const TransientImageResource& resource, size_t offset) = 0;
    virtual void allocate_buffer_resource(const TransientBufferResource& resource, size_t offset) = 0;

    virtual void allocate() = 0;

    virtual size_t get_size() const = 0;
};

} // namespace Mizu