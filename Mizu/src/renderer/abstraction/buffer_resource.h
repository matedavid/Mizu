#pragma once

#include <memory>

namespace Mizu {

// Forward declarations
class IDeviceMemoryAllocator;

using BufferUsageBitsType = uint8_t;

// clang-format off
enum class BufferUsageBits : BufferUsageBitsType {
    None          = 0,
    VertexBuffer  = 1,
    IndexBuffer   = 2,
    UniformBuffer = 4,
    StorageBuffer = 8,
    TransferDst   = 16,
    TransferSrc   = 32,
};
// clang-format on

inline BufferUsageBits operator|(BufferUsageBits a, BufferUsageBits b) {
    return static_cast<BufferUsageBits>(static_cast<BufferUsageBitsType>(a) | static_cast<BufferUsageBitsType>(b));
}

inline BufferUsageBitsType operator&(BufferUsageBits a, BufferUsageBits b) {
    return static_cast<BufferUsageBitsType>(a) & static_cast<BufferUsageBitsType>(b);
}

struct BufferDescription {
    size_t size;
    BufferUsageBits usage;
};

class BufferResource {
  public:
    virtual ~BufferResource() = default;

    static std::shared_ptr<BufferResource> create(const BufferDescription& desc,
                                                  std::weak_ptr<IDeviceMemoryAllocator> allocator);
    static std::shared_ptr<BufferResource> create(const BufferDescription& desc,
                                                  const uint8_t* data,
                                                  std::weak_ptr<IDeviceMemoryAllocator> allocator);

    virtual void set_data(const uint8_t* data) const = 0;

    [[nodiscard]] virtual size_t get_size() const = 0;
    [[nodiscard]] virtual BufferUsageBits get_usage() const = 0;
};

} // namespace Mizu