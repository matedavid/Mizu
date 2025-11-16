#include "buffers.h"

namespace Mizu
{

//
// VertexBuffer
//

VertexBuffer::VertexBuffer(std::shared_ptr<BufferResource> resource, uint32_t type_size)
    : m_resource(std::move(resource))
    , m_type_size(type_size)
{
}

BufferDescription VertexBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDst;
    desc.name = name;

    if (Renderer::get_capabilities().ray_tracing_hardware)
    {
        desc.usage |= BufferUsageBits::RtxAccelerationStructureInputReadOnly;
    }

    return desc;
}

//
// IndexBuffer
//

IndexBuffer::IndexBuffer(std::shared_ptr<BufferResource> resource, uint32_t count)
    : m_resource(std::move(resource))
    , m_count(count)
{
}

std::shared_ptr<IndexBuffer> IndexBuffer::create(const std::vector<uint32_t>& data)
{
    const BufferDescription desc = get_buffer_description(data.size() * sizeof(uint32_t));
    const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());

    const auto resource = BufferResource::create(desc);
    BufferUtils::initialize_buffer(*resource, data_ptr, desc.size);

    return std::make_shared<IndexBuffer>(resource, static_cast<uint32_t>(data.size()));
}

BufferDescription IndexBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::IndexBuffer | BufferUsageBits::TransferDst;
    desc.name = name;

    if (Renderer::get_capabilities().ray_tracing_hardware)
    {
        desc.usage |= BufferUsageBits::RtxAccelerationStructureInputReadOnly;
    }

    return desc;
}

//
// ConstantBuffer
//

ConstantBuffer::ConstantBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

BufferDescription ConstantBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = 0;
    desc.usage = BufferUsageBits::ConstantBuffer | BufferUsageBits::HostVisible;
    desc.name = name;

    return desc;
}

//
// StructuredBuffer
//

StructuredBuffer::StructuredBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

BufferDescription StructuredBuffer::get_buffer_description(uint64_t size, uint64_t stride, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = stride;
    desc.usage = BufferUsageBits::UnorderedAccess | BufferUsageBits::TransferDst;
    desc.name = std::move(name);

    return desc;
}

//
// StagingBuffer
//

StagingBuffer::StagingBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

std::shared_ptr<StagingBuffer> StagingBuffer::create(uint64_t size, const uint8_t* data, std::string name)
{
    const BufferDescription buffer_desc = get_buffer_description(size, name);

    const auto resource = BufferResource::create(buffer_desc);
    resource->set_data(data);

    return std::make_shared<StagingBuffer>(resource);
}

BufferDescription StagingBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::TransferSrc | BufferUsageBits::HostVisible;
    desc.name = std::move(name);

    return desc;
}

} // namespace Mizu
