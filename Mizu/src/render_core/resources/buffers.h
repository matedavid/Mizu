#pragma once

#include <cassert>
#include <glm/glm.hpp>
#include <memory>
#include <utility>
#include <vector>

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/renderer.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

// Forward declarations
class IDeviceMemoryAllocator;

glm::vec2 convert_texture_coords(glm::vec2 coords);

class VertexBuffer
{
  public:
    template <typename T>
    static std::shared_ptr<VertexBuffer> create(const std::vector<T>& data,
                                                std::weak_ptr<IDeviceMemoryAllocator> allocator)
    {
        BufferDescription desc{};
        desc.size = sizeof(T) * data.size();
        desc.type = BufferType::VertexBuffer;

        const auto resource =
            BufferResource::create(desc, reinterpret_cast<const uint8_t*>(data.data()), std::move(allocator));

        auto* value = new VertexBuffer(resource, sizeof(T));
        return std::shared_ptr<VertexBuffer>(value);
    }

    [[nodiscard]] size_t get_count() const { return m_resource->get_size() / m_type_size; }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
    size_t m_type_size = 0;

    VertexBuffer(std::shared_ptr<BufferResource> resource, size_t type_size);
};

class IndexBuffer
{
  public:
    static std::shared_ptr<IndexBuffer> create(const std::vector<uint32_t>& data,
                                               std::weak_ptr<IDeviceMemoryAllocator> allocator);

    [[nodiscard]] size_t get_count() const { return m_count; }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
    size_t m_count = 0;

    IndexBuffer(std::shared_ptr<BufferResource> resource, size_t count);
};

class UniformBuffer
{
  public:
    template <typename T>
    static std::shared_ptr<UniformBuffer> create(std::weak_ptr<IDeviceMemoryAllocator> allocator)
    {
        BufferDescription desc{};
        desc.size = sizeof(T);
        desc.type = BufferType::UniformBuffer;

        const auto resource = BufferResource::create(desc, std::move(allocator));

        auto* value = new UniformBuffer(resource);
        return std::shared_ptr<UniformBuffer>(value);
    }

    template <typename T>
    void update(const T& data)
    {
        MIZU_ASSERT(sizeof(data) == m_resource->get_size(), "Size of data must be equal to size of creation data");
        m_resource->set_data(reinterpret_cast<const uint8_t*>(&data));
    }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;

    UniformBuffer(std::shared_ptr<BufferResource> resource);
};

class StorageBuffer
{
  public:
    template <typename T>
    static std::shared_ptr<StorageBuffer> create(const std::vector<T>& data,
                                                 std::weak_ptr<IDeviceMemoryAllocator> allocator)
    {
        BufferDescription desc{};
        desc.size = sizeof(T) * data.size();
        desc.type = BufferType::StorageBuffer;

        const auto resource = BufferResource::create(desc, std::move(allocator));

        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());
        resource->set_data(data_ptr);

        auto* value = new StorageBuffer(resource);
        return std::shared_ptr<StorageBuffer>(value);
    }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;

    StorageBuffer(std::shared_ptr<BufferResource> resource);
};
;

} // namespace Mizu
