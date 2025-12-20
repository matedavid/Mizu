#pragma once

#include <memory>
#include <ranges>
#include <span>

#include "render_core/rhi/buffer_resource.h"

#include "renderer/renderer.h"

namespace Mizu
{

// Forward declarations
class ImageResource;

namespace BufferUtils
{

void initialize_buffer(const BufferResource& resource, const uint8_t* data, size_t size);
void initialize_image(const ImageResource& resource, const uint8_t* data);

std::shared_ptr<BufferResource> create_and_initialize_buffer(const BufferDescription& desc, std::span<uint8_t> data);

template <typename T>
std::shared_ptr<BufferResource> create_vertex_buffer(std::span<T> data, std::string name = "")
{
    BufferDescription desc{};
    desc.size = sizeof(T) * data.size();
    desc.stride = sizeof(T);
    desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDst;
    desc.name = std::move(name);

    if (g_render_device->get_properties().ray_tracing_hardware)
    {
        desc.usage |= BufferUsageBits::RtxAccelerationStructureInputReadOnly;
    }

    std::span<uint8_t> data_bytes((uint8_t*)data.data(), desc.size);
    return create_and_initialize_buffer(desc, data_bytes);
}

inline std::shared_ptr<BufferResource> create_index_buffer(std::span<const uint32_t> data, std::string name = "")
{
    BufferDescription desc{};
    desc.size = sizeof(uint32_t) * data.size();
    desc.usage = BufferUsageBits::IndexBuffer | BufferUsageBits::TransferDst;
    desc.name = std::move(name);

    if (g_render_device->get_properties().ray_tracing_hardware)
    {
        desc.usage |= BufferUsageBits::RtxAccelerationStructureInputReadOnly;
    }

    std::span<uint8_t> data_bytes((uint8_t*)data.data(), desc.size);
    return create_and_initialize_buffer(desc, data_bytes);
}

template <typename T>
std::shared_ptr<BufferResource> create_constant_buffer(const T& data, std::string name = "")
{
    BufferDescription desc{};
    desc.size = sizeof(T);
    desc.usage = BufferUsageBits::ConstantBuffer | BufferUsageBits::HostVisible;
    desc.name = std::move(name);

    std::span<uint8_t> data_bytes(reinterpret_cast<uint8_t*>(&data), sizeof(T));
    return create_and_initialize_buffer(desc, data_bytes);
}

template <typename T>
std::shared_ptr<BufferResource> create_structured_buffer(std::span<T> data, std::string name = "")
{
    BufferDescription desc{};
    desc.size = sizeof(T) * data.size();
    desc.stride = sizeof(T);
    desc.usage = BufferUsageBits::TransferDst;
    desc.name = std::move(name);

    std::span<uint8_t> data_bytes(reinterpret_cast<uint8_t*>(data.data()), desc.size);
    return create_and_initialize_buffer(desc, data_bytes);
}

} // namespace BufferUtils

} // namespace Mizu