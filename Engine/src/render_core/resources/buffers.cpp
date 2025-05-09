#include "buffers.h"

#include <utility>

namespace Mizu
{

glm::vec2 convert_texture_coords(glm::vec2 coords)
{
    // Default is top left = {0, 0}

    if (Renderer::get_config().graphics_api == GraphicsAPI::OpenGL)
    {
        return {1.0 - coords.x, 1.0 - coords.y};
    }

    return coords;
}

//
// VertexBuffer
//

VertexBuffer::VertexBuffer(std::shared_ptr<BufferResource> resource, uint32_t type_size)
    : m_resource(std::move(resource))
    , m_type_size(type_size)
{
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
    BufferDescription desc{};
    desc.size = data.size() * sizeof(uint32_t);
    desc.type = BufferType::IndexBuffer;

    const auto resource =
        BufferResource::create(desc, reinterpret_cast<const uint8_t*>(data.data()), std::move(allocator));

    auto* value = new IndexBuffer(resource, static_cast<uint32_t>(data.size()));
    return std::shared_ptr<IndexBuffer>(value);
}

//
// UniformBuffer
//

UniformBuffer::UniformBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

//
// StorageBuffer
//

StorageBuffer::StorageBuffer(std::shared_ptr<BufferResource> resource) : m_resource(std::move(resource)) {}

} // namespace Mizu
