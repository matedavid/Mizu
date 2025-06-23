#pragma once

#include <memory>
#include <string>

#include "base/utils/enum_utils.h"

namespace Mizu
{

// Forward declarations
class IDeviceMemoryAllocator;

using BufferUsageBitsType = uint16_t;

// clang-format off
enum class BufferUsageBits : BufferUsageBitsType
{
    None          = 0,
    VertexBuffer  = (1 << 0),
    IndexBuffer   = (1 << 2),
    UniformBuffer = (1 << 3),
    StorageBuffer = (1 << 4),
    TransferSrc   = (1 << 5),
    TransferDst   = (1 << 6),

    RtxAccelerationStructureStorage       = (1 << 7),
    RtxAccelerationStructureInputReadOnly = (1 << 8),
    RtxShaderBindingTable                 = (1 << 9),

    HostVisible = (1 << 10),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(BufferUsageBits, BufferUsageBitsType);

struct BufferDescription
{
    uint64_t size = 1;
    BufferUsageBits usage = BufferUsageBits::None;

    std::string name = "";
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

    virtual uint64_t get_size() const = 0;
    virtual BufferUsageBits get_usage() const = 0;
};

} // namespace Mizu