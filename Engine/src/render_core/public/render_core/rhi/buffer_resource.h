#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/typed_bitset.h"
#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"
#include "render_core/definitions/device_memory.h"
#include "render_core/definitions/resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu
{

using BufferUsageBitsType = uint16_t;

// clang-format off
enum class BufferUsageBits : BufferUsageBitsType
{
    None            = 0,
    VertexBuffer    = (1 << 0),
    IndexBuffer     = (1 << 1),
    ConstantBuffer  = (1 << 2),
    UnorderedAccess = (1 << 3),
    TransferSrc     = (1 << 4),
    TransferDst     = (1 << 5),

    RtxAccelerationStructureStorage       = (1 << 6),
    RtxAccelerationStructureInputReadOnly = (1 << 7),
    RtxShaderBindingTable                 = (1 << 8),

    HostVisible = (1 << 9),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(BufferUsageBits, BufferUsageBitsType);

enum class BufferResourceState
{
    Undefined,
    ShaderReadOnly,
    UnorderedAccess,
    TransferSrc,
    TransferDst,
    AccelStructScratch,
    AccelStructBuildInput,
};

#if MIZU_DEBUG
MIZU_RENDER_CORE_API std::string_view buffer_resource_state_to_string(BufferResourceState state);
#endif

struct BufferDescription
{
    uint64_t size = 1;
    uint64_t stride = 0; // if stride > 0, buffer will be considered StructuredBuffer
    BufferUsageBits usage = BufferUsageBits::None;

    ResourceSharingMode sharing_mode = ResourceSharingMode::Exclusive;
    typed_bitset<CommandBufferType> queue_families{};

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

    MIZU_RENDER_CORE_API void set_data(const uint8_t* data, uint64_t size, uint64_t offset) const;
    MIZU_RENDER_CORE_API void set_data(const uint8_t* data) const;

    virtual uint8_t* get_mapped_data() const = 0;
    virtual uint8_t* map() = 0;
    virtual void unmap() = 0;

    virtual MemoryRequirements get_memory_requirements() const = 0;

    virtual uint64_t get_size() const = 0;
    virtual uint64_t get_stride() const = 0;
    virtual BufferUsageBits get_usage() const = 0;
    virtual ResourceSharingMode get_sharing_mode() const = 0;

    virtual const std::string& get_name() const = 0;
};

MIZU_RENDER_CORE_API BufferDescription create_constant_buffer_desc(uint64_t size, std::string name = "");
MIZU_RENDER_CORE_API BufferDescription
create_structured_buffer_desc(uint64_t size, uint64_t stride, std::string name = "");
MIZU_RENDER_CORE_API BufferDescription create_vertex_buffer_desc(uint64_t size, uint64_t stride, std::string name = "");
MIZU_RENDER_CORE_API BufferDescription create_index_buffer_desc(uint64_t size, std::string name = "");
MIZU_RENDER_CORE_API BufferDescription create_staging_buffer_desc(uint64_t size, std::string name = "");

template <typename T>
BufferDescription create_constant_buffer_desc(std::string name = "")
{
    return create_constant_buffer_desc(sizeof(T), name);
}

template <typename T>
BufferDescription create_structured_buffer_desc(uint64_t num, std::string name = "")
{
    return create_structured_buffer_desc(num * sizeof(T), sizeof(T), name);
}

template <typename T>
BufferDescription create_vertex_buffer_desc(uint64_t num, std::string name = "")
{
    return create_vertex_buffer_desc(num * sizeof(T), sizeof(T), name);
}

} // namespace Mizu
