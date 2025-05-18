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

class VertexBuffer
{
  public:
    VertexBuffer(std::shared_ptr<BufferResource> resource, uint64_t type_size);

    template <typename T>
    static std::shared_ptr<VertexBuffer> create(const std::vector<T>& data,
                                                std::weak_ptr<IDeviceMemoryAllocator> allocator)
    {
        const BufferDescription desc = get_buffer_description(sizeof(T) * data.size());

        const auto resource =
            BufferResource::create(desc, reinterpret_cast<const uint8_t*>(data.data()), std::move(allocator));

        return std::make_shared<VertexBuffer>(resource, sizeof(T));
    }

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    [[nodiscard]] uint32_t get_count() const { return static_cast<uint32_t>(m_resource->get_size() / m_type_size); }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
    uint64_t m_type_size = 0;
};

class IndexBuffer
{
  public:
    IndexBuffer(std::shared_ptr<BufferResource> resource, uint32_t count);

    static std::shared_ptr<IndexBuffer> create(const std::vector<uint32_t>& data,
                                               std::weak_ptr<IDeviceMemoryAllocator> allocator);

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    [[nodiscard]] uint32_t get_count() const { return m_count; }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
    uint32_t m_count = 0;
};

class UniformBuffer
{
  public:
    UniformBuffer(std::shared_ptr<BufferResource> resource);

    template <typename T>
    static std::shared_ptr<UniformBuffer> create(std::weak_ptr<IDeviceMemoryAllocator> allocator, std::string name = "")
    {
        const BufferDescription desc = get_buffer_description(sizeof(T), name);

        const auto resource = BufferResource::create(desc, std::move(allocator));

        auto* value = new UniformBuffer(resource);
        return std::shared_ptr<UniformBuffer>(value);
    }

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    template <typename T>
    void update(const T& data)
    {
        MIZU_ASSERT(sizeof(data) == m_resource->get_size(), "Size of data must be equal to size of creation data");
        m_resource->set_data(reinterpret_cast<const uint8_t*>(&data));
    }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
};

class StorageBuffer
{
  public:
    StorageBuffer(std::shared_ptr<BufferResource> resource);

    template <typename T>
    static std::shared_ptr<StorageBuffer> create(const std::vector<T>& data,
                                                 std::weak_ptr<IDeviceMemoryAllocator> allocator,
                                                 std::string name = "")
    {
        const BufferDescription desc = get_buffer_description(sizeof(T) * data.size(), name);

        const auto resource = BufferResource::create(desc, std::move(allocator));

        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());
        resource->set_data(data_ptr);

        auto* value = new StorageBuffer(resource);
        return std::shared_ptr<StorageBuffer>(value);
    }

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
};

class StagingBuffer
{
  public:
    StagingBuffer(std::shared_ptr<BufferResource> resource);

    static std::shared_ptr<StagingBuffer> create(uint64_t size,
                                                 const uint8_t* data,
                                                 std::weak_ptr<IDeviceMemoryAllocator> allocator,
                                                 std::string name = "");

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
};
;

} // namespace Mizu
