#include "render/frame_linear_allocator.h"

#include "base/debug/assert.h"
#include "render_core/rhi/buffer_resource.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

//
// FrameAllocation
//

void FrameAllocation::upload(std::span<const uint8_t> data) const
{
    MIZU_ASSERT(
        data.size() <= view.desc.size,
        "Trying to upload data with size {} when allocation is of size {}",
        data.size(),
        view.desc.size);

    view.buffer->set_data(data.data(), data.size(), view.desc.offset);
}

//
// FrameLinearAllocator
//

FrameLinearAllocator::FrameLinearAllocator(uint32_t num_frames, uint64_t size_bytes_per_frame, std::string_view name)
    : m_num_frames(num_frames)
    , m_size_per_frame(size_bytes_per_frame)
{
    const uint64_t total_size = m_num_frames * m_size_per_frame;

    typed_bitset<CommandBufferType> queue_families{};
    queue_families.set(CommandBufferType::Graphics);
    queue_families.set(CommandBufferType::Compute);
    queue_families.set(CommandBufferType::Transfer);

    // clang-format off
    constexpr BufferUsageBits USAGE_BITS = BufferUsageBits::HostVisible
                                         | BufferUsageBits::ConstantBuffer
                                         | BufferUsageBits::ShaderResource
                                         | BufferUsageBits::TransferSrc;
    // clang-format on

    BufferDescription buffer_desc{};
    buffer_desc.size = total_size;
    buffer_desc.usage = USAGE_BITS;
    buffer_desc.sharing_mode = ResourceSharingMode::Concurrent;
    buffer_desc.queue_families = queue_families;
    buffer_desc.name = name;

    m_buffer = g_render_device->create_buffer(buffer_desc);
}

void FrameLinearAllocator::prepare_frame(uint32_t frame_in_flight_idx)
{
    MIZU_ASSERT(
        frame_in_flight_idx < m_num_frames,
        "Invalid frame number {} when the number of available frames is {}",
        frame_in_flight_idx,
        m_num_frames);

    m_frame_in_flight_idx = frame_in_flight_idx;
    m_frame_in_flight_idx_head = m_frame_in_flight_idx * m_size_per_frame;
}

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

FrameAllocation FrameLinearAllocator::allocate(uint64_t size, uint64_t alignment, uint32_t stride)
{
    const uint64_t offset = align_up(m_frame_in_flight_idx_head, alignment);

    MIZU_ASSERT(offset + size <= (m_frame_in_flight_idx + 1) * m_size_per_frame, "Overflowing allocated frame size");

    m_frame_in_flight_idx_head = offset + size;

    BufferResourceViewDescription view_desc{};
    view_desc.offset = offset;
    view_desc.size = size;
    view_desc.stride = stride;

    FrameAllocation allocation{};
    allocation.view = BufferResourceView::create(m_buffer, view_desc);
    allocation.frame_in_flight_idx = m_frame_in_flight_idx;

    return allocation;
}

} // namespace Mizu