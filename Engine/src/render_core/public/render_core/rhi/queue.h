#pragma once

#include <glm/glm.hpp>
#include <span>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/gpu_heap.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/synchronization.h"

namespace Mizu
{

struct SubmitInfo
{
    static constexpr size_t MAX_WAITS = 8;
    static constexpr size_t MAX_SIGNALS = 4;

    inplace_vector<SyncPoint, MAX_WAITS> waits{};
    inplace_vector<SyncPoint, MAX_SIGNALS> signals{};
};

struct SparseBufferMapping
{
    BufferResource* buffer = nullptr;

    uint64_t resource_offset = 0;
    uint64_t size = 0;

    MemoryPlacement memory{};
};

struct SparseImageMapping
{
    ImageResource* image = nullptr;

    // VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_level = 0;
    uint32_t array_layer = 0;

    // Values in texels
    glm::uvec3 offset{};
    glm::uvec3 extent{};

    MemoryPlacement memory{};
};

class SparseBindBatch
{
  public:
    SparseBindBatch() = default;

    SparseBindBatch& add_buffer_bind(const SparseBufferMapping& mapping)
    {
        MIZU_ASSERT(
            mapping.resource_offset + mapping.size <= mapping.buffer->get_size(),
            "Sparse buffer mapping exceeds buffer size");

        m_buffer_mappings.push_back(mapping);
        return *this;
    }

    SparseBindBatch& add_image_bind(const SparseImageMapping& mapping)
    {
        MIZU_ASSERT(
            mapping.offset.x + mapping.extent.x <= mapping.image->get_width()
                && mapping.offset.y + mapping.extent.y <= mapping.image->get_height()
                && mapping.offset.z + mapping.extent.z <= mapping.image->get_depth(),
            "Sparse image mapping exceeds image dimensions");

        m_image_mappings.push_back(mapping);
        return *this;
    }

    std::span<const SparseBufferMapping> get_buffer_mappings() const { return m_buffer_mappings; }
    std::span<const SparseImageMapping> get_image_mappings() const { return m_image_mappings; }

  private:
    std::vector<SparseBufferMapping> m_buffer_mappings{};
    std::vector<SparseImageMapping> m_image_mappings{};
};

class Queue
{
  public:
    virtual ~Queue() = default;

    virtual CommandBufferType get_type() const = 0;

    virtual SyncPoint submit(std::span<CommandBuffer*> command_buffers, const SubmitInfo& info = {}) = 0;

    SyncPoint submit(CommandBuffer& command_buffer, const SubmitInfo& info = {})
    {
        CommandBuffer* command_buffer_ptr = &command_buffer;
        return submit(std::span<CommandBuffer*>(&command_buffer_ptr, 1), info);
    }

    virtual SyncPoint bind_sparse(const SparseBindBatch& batch, const SubmitInfo& info = {}) = 0;

    virtual void wait(const SyncPoint& point) = 0;

    virtual SyncPoint get_last_submitted() const = 0;
    virtual void wait_idle() = 0;
};

} // namespace Mizu
