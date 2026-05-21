#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "asset/asset_handle.h"
#include "render_core/rhi/buffer_resource.h"

#include "render/resources/gpu_pools_handles.h"

namespace Mizu
{

class BufferRangeAllocator
{
  public:
    bool init(uint64_t size);

    std::optional<uint64_t> allocate(uint64_t size, uint64_t alignment = alignof(std::max_align_t));
    void free(uint64_t offset, uint64_t size);

  private:
    struct FreeRange
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    uint64_t m_capacity = 0;
    std::mutex m_mutex;
    std::vector<FreeRange> m_free_ranges;
};

class GpuMeshPool
{
  public:
    GpuMeshPool() = default;

    bool init(uint64_t size);

    std::optional<GpuMeshAllocationHandle> allocate(
        const MeshAssetHandle& handle,
        uint64_t vertex_size,
        uint64_t index_size,
        uint64_t vertex_alignment = alignof(std::max_align_t),
        uint64_t index_alignment = alignof(std::max_align_t));

    void free(const GpuMeshAllocationHandle& allocation);

    const std::shared_ptr<BufferResource>& get_vertex_buffer() const { return m_vertex_buffer; }
    const std::shared_ptr<BufferResource>& get_index_buffer() const { return m_index_buffer; }

  private:
    std::shared_ptr<BufferResource> m_vertex_buffer = nullptr;
    std::shared_ptr<BufferResource> m_index_buffer = nullptr;

    BufferRangeAllocator m_vertex_allocator;
    BufferRangeAllocator m_index_allocator;
};

class GpuTexturePool
{
  public:
    GpuTexturePool() = default;

    bool init(uint64_t size);

    void free(const GpuTextureAllocationHandle& allocation) { (void)allocation; };
};

} // namespace Mizu
