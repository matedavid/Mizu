#pragma once

#include <memory>

namespace Mizu
{

// Forward declarations
class IDeviceMemoryAllocator;

enum class BufferType
{
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    StorageBuffer,
    Staging,
};

struct BufferDescription
{
    uint64_t size = 1;
    BufferType type = BufferType::UniformBuffer;
};

class BufferResource
{
  public:
    virtual ~BufferResource() = default;

    static std::shared_ptr<BufferResource> create(const BufferDescription& desc,
                                                  std::weak_ptr<IDeviceMemoryAllocator> allocator);
    static std::shared_ptr<BufferResource> create(const BufferDescription& desc,
                                                  const uint8_t* data,
                                                  std::weak_ptr<IDeviceMemoryAllocator> allocator);

    virtual void set_data(const uint8_t* data) const = 0;

    [[nodiscard]] virtual uint64_t get_size() const = 0;
    [[nodiscard]] virtual BufferType get_type() const = 0;
};

} // namespace Mizu