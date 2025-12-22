#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"
#include "render_core/definitions/device_memory.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu
{

using BufferUsageBitsType = uint16_t;

// clang-format off
enum class BufferUsageBits : BufferUsageBitsType
{
    None            = 0,
    VertexBuffer    = (1 << 0),
    IndexBuffer     = (1 << 2),
    ConstantBuffer  = (1 << 3),
    UnorderedAccess = (1 << 4),
    TransferSrc     = (1 << 5),
    TransferDst     = (1 << 6),

    RtxAccelerationStructureStorage       = (1 << 7),
    RtxAccelerationStructureInputReadOnly = (1 << 8),
    RtxShaderBindingTable                 = (1 << 9),

    HostVisible = (1 << 10),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(BufferUsageBits, BufferUsageBitsType);

enum class BufferResourceState
{
    Undefined,
    UnorderedAccess,
    TransferSrc,
    TransferDst,
    ShaderReadOnly,
};

#if MIZU_DEBUG
MIZU_RENDER_CORE_API std::string_view buffer_resource_state_to_string(BufferResourceState state);
#endif

struct BufferDescription
{
    uint64_t size = 1;
    uint64_t stride = 0; // if stride > 0, buffer will be considered StructuredBuffer
    BufferUsageBits usage = BufferUsageBits::None;

    bool is_virtual = false;

    std::string name = "";
};

class BufferResource
{
  public:
    virtual ~BufferResource() = default;

    virtual ResourceView as_srv() = 0;
    virtual ResourceView as_uav() = 0;
    virtual ResourceView as_cbv() = 0;

    void set_data(const uint8_t* data) const { set_data(data, get_size(), 0); }
    virtual void set_data(const uint8_t* data, size_t size, size_t offset) const = 0;

    virtual MemoryRequirements get_memory_requirements() const = 0;

    virtual uint64_t get_size() const = 0;
    virtual uint64_t get_stride() const = 0;
    virtual BufferUsageBits get_usage() const = 0;

    virtual const std::string& get_name() const = 0;
};

} // namespace Mizu