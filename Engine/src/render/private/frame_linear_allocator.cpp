#include "render/frame_linear_allocator.h"

#include "base/debug/assert.h"
#include "render_core/rhi/buffer_resource.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

//
// FrameAllocation
//

void FrameAllocation::upload(std::span<const uint8_t> data)
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

FrameLinearAllocator::FrameLinearAllocator(uint32_t num_frames, uint64_t size_bytes_per_frame)
    : m_num_frames(num_frames)
    , m_size_per_frame(size_bytes_per_frame)
{
    const uint64_t total_size = m_num_frames * m_size_per_frame;

    typed_bitset<CommandBufferType> queue_families{};
    queue_families.set(CommandBufferType::Graphics);
    queue_families.set(CommandBufferType::Compute);
    queue_families.set(CommandBufferType::Transfer);

    BufferDescription buffer_desc{};
    buffer_desc.size = total_size;
    buffer_desc.usage =
        BufferUsageBits::HostVisible | BufferUsageBits::ConstantBuffer | BufferUsageBits::ShaderResource;
    buffer_desc.sharing_mode = ResourceSharingMode::Concurrent;
    buffer_desc.queue_families = queue_families;
    buffer_desc.name = "FrameLinearAllocator";

    m_buffer = g_render_device->create_buffer(buffer_desc);
}

void FrameLinearAllocator::prepare_frame()
{
    m_current_frame = (m_current_frame + 1) % m_num_frames;
    m_current_frame_head = m_current_frame * m_size_per_frame;
}

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

FrameAllocation FrameLinearAllocator::allocate(uint64_t size, uint64_t alignment, uint32_t stride)
{
    const uint64_t offset = align_up(m_current_frame_head, alignment);

    MIZU_ASSERT(offset + size <= (m_current_frame + 1) * m_size_per_frame, "Overflowing allocated frame size");

    m_current_frame_head = offset + size;

    BufferResourceViewDescription view_desc{};
    view_desc.offset = offset;
    view_desc.size = size;
    view_desc.stride = stride;

    FrameAllocation allocation{};
    allocation.view = BufferResourceView::create(m_buffer, view_desc);
    allocation.frame_num = m_current_frame;

    return allocation;
}

} // namespace Mizu