#include "buffers.h"

#include <utility>

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

std::shared_ptr<IndexBuffer> IndexBuffer::create(const std::vector<uint32_t>& data,
                                                 std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    const BufferDescription desc = get_buffer_description(data.size() * sizeof(uint32_t));

    const auto resource =
        BufferResource::create(desc, reinterpret_cast<const uint8_t*>(data.data()), std::move(allocator));
    return std::make_shared<IndexBuffer>(resource, static_cast<uint32_t>(data.size()));
}

BufferDescription IndexBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::IndexBuffer | BufferUsageBits::TransferDst;
    desc.name = name;

    return desc;
}

//
// UniformBuffer
//

UniformBuffer::UniformBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

BufferDescription UniformBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::UniformBuffer | BufferUsageBits::HostVisible;
    desc.name = name;

    return desc;
}

//
// StorageBuffer
//

StorageBuffer::StorageBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

BufferDescription StorageBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::StorageBuffer | BufferUsageBits::TransferDst;
    desc.name = name;

    return desc;
}

//
// StagingBuffer
//

StagingBuffer::StagingBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

std::shared_ptr<StagingBuffer> StagingBuffer::create(uint64_t size,
                                                     const uint8_t* data,
                                                     std::weak_ptr<IDeviceMemoryAllocator> allocator,
                                                     std::string name)
{
    const BufferDescription buffer_desc = get_buffer_description(size, name);

    const auto resource = BufferResource::create(buffer_desc, data, std::move(allocator));
    return std::make_shared<StagingBuffer>(resource);
}

BufferDescription StagingBuffer::get_buffer_description(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.usage = BufferUsageBits::TransferSrc | BufferUsageBits::HostVisible;
    desc.name = name;

    return desc;
}

} // namespace Mizu
