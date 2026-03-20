#pragma once

#include <memory>
#include <span>

#include "render_core/rhi/buffer_resource.h"

#include "mizu_render_module.h"
#include "render/runtime/renderer.h"

namespace Mizu
{

// Forward declarations
class ImageResource;

namespace BufferUtils
{

MIZU_RENDER_API void initialize_buffer(const BufferResource& resource, const uint8_t* data, size_t size);
MIZU_RENDER_API void initialize_image(const ImageResource& resource, const uint8_t* data);

MIZU_RENDER_API std::shared_ptr<BufferResource> create_and_initialize_buffer(
    const BufferDescription& desc,
    std::span<const uint8_t> data);

template <typename T>
std::shared_ptr<BufferResource> create_vertex_buffer(std::span<const T> data, std::string name = "")
{
    BufferDescription desc = create_vertex_buffer_desc<T>(data.size(), std::move(name));
    desc.usage |= BufferUsageBits::TransferDst;

    if (g_render_device->get_properties().ray_tracing_hardware)
    {
        desc.usage |= BufferUsageBits::RtxAccelerationStructureInputReadOnly;
    }

    std::span<const uint8_t> data_bytes(reinterpret_cast<const uint8_t*>(data.data()), desc.size);
    return create_and_initialize_buffer(desc, data_bytes);
}

inline std::shared_ptr<BufferResource> create_index_buffer(std::span<const uint32_t> data, std::string name = "")
{
    BufferDescription desc = create_index_buffer_desc(data.size() * sizeof(uint32_t), std::move(name));
    desc.usage |= BufferUsageBits::TransferDst;

    if (g_render_device->get_properties().ray_tracing_hardware)
    {
        desc.usage |= BufferUsageBits::RtxAccelerationStructureInputReadOnly;
    }

    std::span<const uint8_t> data_bytes(reinterpret_cast<const uint8_t*>(data.data()), desc.size);
    return create_and_initialize_buffer(desc, data_bytes);
}

template <typename T>
std::shared_ptr<BufferResource> create_constant_buffer(const T& data, std::string name = "")
{
    const BufferDescription desc = create_constant_buffer_desc<T>(std::move(name));

    std::span<const uint8_t> data_bytes(reinterpret_cast<const uint8_t*>(&data), sizeof(T));

    const auto buffer = g_render_device->create_buffer(desc);
    buffer->set_data(data_bytes.data(), desc.size, 0);

    return buffer;
}

template <typename T>
std::shared_ptr<BufferResource> create_structured_buffer(std::span<const T> data, std::string name = "")
{
    BufferDescription desc = create_structured_buffer_desc<T>(data.size(), std::move(name));
    desc.usage |= BufferUsageBits::TransferDst;

    std::span<const uint8_t> data_bytes(reinterpret_cast<const uint8_t*>(data.data()), desc.size);
    return create_and_initialize_buffer(desc, data_bytes);
}

inline std::shared_ptr<BufferResource> create_byte_address_buffer(std::span<const uint32_t> data, std::string name = "")
{
    BufferDescription desc = create_byte_address_buffer_desc(data.size(), std::move(name));
    desc.usage |= BufferUsageBits::TransferDst;

    std::span<const uint8_t> data_bytes(reinterpret_cast<const uint8_t*>(data.data()), desc.size);
    return create_and_initialize_buffer(desc, data_bytes);
}

} // namespace BufferUtils

} // namespace Mizu
