#pragma once

#include <cassert>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "mizu_render_core_module.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

class MIZU_RENDER_CORE_API VertexBuffer
{
  public:
    VertexBuffer(std::shared_ptr<BufferResource> resource, uint32_t type_size);

    template <typename T>
    static std::shared_ptr<VertexBuffer> create(const std::vector<T>& data)
    {
        const BufferDescription desc = get_buffer_description(sizeof(T) * data.size());
        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());

        const auto resource = BufferResource::create(desc);
        BufferUtils::initialize_buffer(*resource, data_ptr, desc.size);

        return std::make_shared<VertexBuffer>(resource, static_cast<uint32_t>(sizeof(T)));
    }

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    uint32_t get_count() const { return static_cast<uint32_t>(get_size()) / m_type_size; }

    uint64_t get_size() const { return m_resource->get_size(); }
    uint32_t get_stride() const { return m_type_size; }

    std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
    uint32_t m_type_size = 0;
};

class MIZU_RENDER_CORE_API IndexBuffer
{
  public:
    IndexBuffer(std::shared_ptr<BufferResource> resource, uint32_t count);

    static std::shared_ptr<IndexBuffer> create(const std::vector<uint32_t>& data);

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    uint32_t get_count() const { return m_count; }
    std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
    uint32_t m_count = 0;
};

class MIZU_RENDER_CORE_API ConstantBuffer
{
  public:
    ConstantBuffer(std::shared_ptr<BufferResource> resource);

    template <typename T>
    static std::shared_ptr<ConstantBuffer> create(std::string name = "")
    {
        const BufferDescription desc = get_buffer_description(sizeof(T), name);

        const auto resource = BufferResource::create(desc);
        return std::make_shared<ConstantBuffer>(resource);
    }

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    template <typename T>
    void update(const T& data)
    {
        MIZU_ASSERT(sizeof(data) == m_resource->get_size(), "Size of data must be equal to size of creation data");
        m_resource->set_data(reinterpret_cast<const uint8_t*>(&data));
    }

    std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
};

class MIZU_RENDER_CORE_API StructuredBuffer
{
  public:
    StructuredBuffer(std::shared_ptr<BufferResource> resource);

    template <typename T>
    static std::shared_ptr<StructuredBuffer> create(uint64_t number, std::string name = "")
    {
        const BufferDescription desc = get_buffer_description(number * sizeof(T), sizeof(T), name);

        const auto resource = BufferResource::create(desc);
        return std::make_shared<StructuredBuffer>(resource);
    }

    template <typename T>
    static std::shared_ptr<StructuredBuffer> create(std::span<const T> data, std::string name = "")
    {
        const std::shared_ptr<StructuredBuffer> structured = create<T>(data.size(), std::move(name));
        const BufferResource& resource = *structured->get_resource();

        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());
        BufferUtils::initialize_buffer(resource, data_ptr, resource.get_size());

        return structured;
    }

    static BufferDescription get_buffer_description(uint64_t size, uint64_t stride, std::string name = "");

    std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
};

class MIZU_RENDER_CORE_API StagingBuffer
{
  public:
    StagingBuffer(std::shared_ptr<BufferResource> resource);

    static std::shared_ptr<StagingBuffer> create(uint64_t size, const uint8_t* data, std::string name = "");

    static BufferDescription get_buffer_description(uint64_t size, std::string name = "");

    std::shared_ptr<BufferResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<BufferResource> m_resource;
};

} // namespace Mizu
