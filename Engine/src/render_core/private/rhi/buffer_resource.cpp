#include "render_core/rhi/buffer_resource.h"

#include <cstring>

namespace Mizu
{

#if MIZU_DEBUG

std::string_view buffer_resource_state_to_string(BufferResourceState state)
{
    switch (state)
    {
    case BufferResourceState::Undefined:
        return "Undefined";
    case BufferResourceState::ShaderReadOnly:
        return "ShaderReadOnly";
    case BufferResourceState::UnorderedAccess:
        return "UnorderedAccess";
    case BufferResourceState::TransferSrc:
        return "TransferSrc";
    case BufferResourceState::TransferDst:
        return "TransferDst";
    case BufferResourceState::AccelStructScratch:
        return "AccelStructScratch";
    case BufferResourceState::AccelStructBuildInput:
        return "AccelStructBuildInput";
    }
}

#endif

void BufferResource::set_data(const uint8_t* data, uint64_t size, uint64_t offset) const
{
    MIZU_ASSERT(offset + size <= get_size(), "Trying to set data outside of buffer bounds");

    uint8_t* mapped_data = get_mapped_data();
    MIZU_ASSERT(mapped_data != nullptr, "Failed to map buffer resource '{}'", get_name());

    memcpy(mapped_data + offset, data, size);
}

void BufferResource::set_data(const uint8_t* data) const
{
    set_data(data, get_size(), 0);
}

BufferDescription create_constant_buffer_desc(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = 0;
    desc.usage = BufferUsageBits::ConstantBuffer | BufferUsageBits::HostVisible;
    desc.name = std::move(name);

    return desc;
}

BufferDescription create_structured_buffer_desc(uint64_t size, uint32_t stride, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = stride;
    desc.usage = BufferUsageBits::ShaderResource;
    desc.name = std::move(name);

    return desc;
}

BufferDescription create_byte_address_buffer_desc(uint64_t num, std::string name)
{
    BufferDescription desc{};
    desc.size = num * sizeof(uint32_t);
    desc.stride = 0;
    desc.usage = BufferUsageBits::ShaderResource;
    desc.name = std::move(name);

    return desc;
}

BufferDescription create_vertex_buffer_desc(uint64_t size, uint32_t stride, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = stride;
    desc.usage = BufferUsageBits::VertexBuffer;
    desc.name = std::move(name);

    return desc;
}

BufferDescription create_index_buffer_desc(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::IndexBuffer;
    desc.name = std::move(name);

    return desc;
}

BufferDescription create_staging_buffer_desc(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::TransferSrc | BufferUsageBits::HostVisible;
    desc.name = std::move(name);

    return desc;
}

} // namespace Mizu
