#pragma once

#include <memory>
#include <span>

#include "render_core/rhi/resource_view.h"

#include "mizu_render_module.h"

namespace Mizu
{

class BufferResource;

struct FrameAllocation
{
    BufferResourceView view;
    uint32_t frame_num;

    MIZU_RENDER_API void upload(std::span<const uint8_t> data);

    template <typename T>
    void upload(const T& data)
    {
        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&data);
        upload(std::span(data_ptr, sizeof(T)));
    }
};

class MIZU_RENDER_API FrameLinearAllocator
{
  public:
    FrameLinearAllocator(uint32_t num_frames, uint64_t size_bytes_per_frame);

    void prepare_frame();

    template <typename T>
    FrameAllocation allocate()
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for GPU upload");
        return allocate(sizeof(T), alignof(T));
    }

    FrameAllocation allocate(uint64_t size, uint64_t alignment);

  private:
    std::shared_ptr<BufferResource> m_buffer;

    uint32_t m_num_frames;
    uint64_t m_size_per_frame;

    uint32_t m_current_frame = 0;
    uint64_t m_current_frame_head = 0;
};

} // namespace Mizu