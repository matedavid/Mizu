#pragma once

#include <algorithm>
#include <memory>
#include <numeric>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>

#include "render/runtime/renderer.h"
#include "render_core/rhi/resource_view.h"

#include "mizu_render_module.h"

namespace Mizu
{

class BufferResource;

struct FrameAllocation
{
    BufferResourceView view;
    uint32_t frame_num;

    MIZU_RENDER_API void upload(std::span<const uint8_t> data) const;

    template <typename RangeT>
        requires(std::ranges::contiguous_range<RangeT>)
    void upload(const RangeT& data) const
    {
        using T = std::ranges::range_value_t<RangeT>;

        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());
        upload(std::span(data_ptr, data.size() * sizeof(T)));
    }

    template <typename T>
        requires(!std::ranges::contiguous_range<T>)
    void upload(const T& data) const
    {
        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&data);
        upload(std::span(data_ptr, sizeof(T)));
    }
};

class MIZU_RENDER_API FrameLinearAllocator
{
  public:
    FrameLinearAllocator(
        uint32_t num_frames,
        uint64_t size_bytes_per_frame,
        std::string_view name = "FrameLinearAllocator");

    void prepare_frame(uint32_t frame_num);

    template <typename T>
    FrameAllocation allocate_constant()
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        const uint64_t alignment =
            std::max(alignof(T), g_render_device->get_properties().min_constant_buffer_offset_alignment);
        return allocate(sizeof(T), alignment, 0);
    }

    template <typename T>
    FrameAllocation allocate_structured(uint64_t count)
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        const uint64_t alignment =
            std::lcm(sizeof(T), g_render_device->get_properties().min_raw_buffer_offset_alignment);
        return allocate(sizeof(T) * count, alignment, sizeof(T));
    }

    FrameAllocation allocate_byte_address(uint64_t size)
    {
        return allocate(size, g_render_device->get_properties().min_raw_buffer_offset_alignment, 0u);
    }

    FrameAllocation allocate(uint64_t size, uint64_t alignment, uint32_t stride);

  private:
    std::shared_ptr<BufferResource> m_buffer;

    uint32_t m_num_frames;
    uint64_t m_size_per_frame;

    uint32_t m_current_frame = 0;
    uint64_t m_current_frame_head = 0;
};

} // namespace Mizu