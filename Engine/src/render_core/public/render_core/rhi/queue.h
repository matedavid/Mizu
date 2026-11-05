#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <span>
#include <vector>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"

#include "mizu_render_core_module.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/gpu_heap.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/synchronization.h"

namespace Mizu
{

struct SparseBufferBind
{
    BufferResource* buffer = nullptr;

    uint64_t resource_offset = 0;
    uint64_t size = 0;

    MemoryPlacement memory{};
};

struct SparseBufferUnbind
{
    BufferResource* buffer = nullptr;

    uint64_t resource_offset = 0;
    uint64_t size = 0;
};

struct SparseImageBind
{
    ImageResource* image = nullptr;

    uint32_t mip_level = 0;
    uint32_t array_layer = 0;

    // Values in texels, tile aligned
    glm::uvec3 offset{};
    glm::uvec3 extent{};

    MemoryPlacement memory{};
};

struct SparseImageUnbind
{
    ImageResource* image = nullptr;

    uint32_t mip_level = 0;
    uint32_t array_layer = 0;

    glm::uvec3 offset{};
    glm::uvec3 extent{};
};

struct SparseMipTailBind
{
    ImageResource* image = nullptr;
    // Must be 0 if the image has a single mip tail
    uint32_t array_layer = 0;

    MemoryPlacement memory{};
};

struct SparseMipTailUnbind
{
    ImageResource* image = nullptr;
    uint32_t array_layer = 0;
};

struct SparseMetadataBind
{
    ImageResource* image = nullptr;

    MemoryPlacement memory{};
};

struct SparseMetadataUnbind
{
    ImageResource* image = nullptr;
};

class SparseBindBatch
{
  public:
    SparseBindBatch() = default;

    MIZU_RENDER_CORE_API SparseBindBatch& add_buffer_bind(const SparseBufferBind& bind);
    MIZU_RENDER_CORE_API SparseBindBatch& add_buffer_unbind(const SparseBufferUnbind& unbind);
    MIZU_RENDER_CORE_API SparseBindBatch& add_image_bind(const SparseImageBind& bind);
    MIZU_RENDER_CORE_API SparseBindBatch& add_image_unbind(const SparseImageUnbind& unbind);
    MIZU_RENDER_CORE_API SparseBindBatch& add_mip_tail_bind(const SparseMipTailBind& bind);
    MIZU_RENDER_CORE_API SparseBindBatch& add_mip_tail_unbind(const SparseMipTailUnbind& unbind);
    MIZU_RENDER_CORE_API SparseBindBatch& add_metadata_bind(const SparseMetadataBind& bind);
    MIZU_RENDER_CORE_API SparseBindBatch& add_metadata_unbind(const SparseMetadataUnbind& unbind);

    bool has_binds() const
    {
        return !m_buffer_binds.empty() || !m_image_binds.empty() || !m_mip_tail_binds.empty()
               || !m_metadata_binds.empty();
    }

    bool has_unbinds() const
    {
        return !m_buffer_unbinds.empty() || !m_image_unbinds.empty() || !m_mip_tail_unbinds.empty()
               || !m_metadata_unbinds.empty();
    }

    bool is_empty() const { return !has_binds() && !has_unbinds(); }

    std::span<const SparseBufferBind> get_buffer_binds() const { return m_buffer_binds; }
    std::span<const SparseBufferUnbind> get_buffer_unbinds() const { return m_buffer_unbinds; }
    std::span<const SparseImageBind> get_image_binds() const { return m_image_binds; }
    std::span<const SparseImageUnbind> get_image_unbinds() const { return m_image_unbinds; }
    std::span<const SparseMipTailBind> get_mip_tail_binds() const { return m_mip_tail_binds; }
    std::span<const SparseMipTailUnbind> get_mip_tail_unbinds() const { return m_mip_tail_unbinds; }
    std::span<const SparseMetadataBind> get_metadata_binds() const { return m_metadata_binds; }
    std::span<const SparseMetadataUnbind> get_metadata_unbinds() const { return m_metadata_unbinds; }

  private:
    std::vector<SparseBufferBind> m_buffer_binds{};
    std::vector<SparseBufferUnbind> m_buffer_unbinds{};
    std::vector<SparseImageBind> m_image_binds{};
    std::vector<SparseImageUnbind> m_image_unbinds{};
    std::vector<SparseMipTailBind> m_mip_tail_binds{};
    std::vector<SparseMipTailUnbind> m_mip_tail_unbinds{};
    std::vector<SparseMetadataBind> m_metadata_binds{};
    std::vector<SparseMetadataUnbind> m_metadata_unbinds{};

    // A resource range can only appear once in a batch, Vulkan does not allow binding a range more than once in the
    // same VkBindSparseInfo. Remapping a range only requires a bind, it replaces the previous binding.
    bool overlaps_buffer_range(const BufferResource* buffer, uint64_t resource_offset, uint64_t size) const;
    bool overlaps_image_box(
        const ImageResource* image,
        uint32_t mip_level,
        uint32_t array_layer,
        glm::uvec3 offset,
        glm::uvec3 extent) const;
    bool overlaps_mip_tail(const ImageResource* image, uint32_t array_layer) const;
    bool overlaps_metadata(const ImageResource* image) const;
};

struct SubmitInfo
{
    static constexpr size_t MAX_WAITS = 8;
    static constexpr size_t MAX_SIGNALS = 4;

    inplace_vector<SyncPoint, MAX_WAITS> waits{};
    inplace_vector<SyncPoint, MAX_SIGNALS> signals{};
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
