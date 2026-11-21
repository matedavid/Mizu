#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "mizu_render_core_module.h"
#include "render_core/allocators/gpu_allocator.h"

namespace Mizu
{

struct GpuBlockAllocatorDescription
{
    uint64_t device_local_block_size = 256ull * 1024 * 1024;
    uint64_t host_visible_block_size = 64ull * 1024 * 1024;

    std::string name = "GpuBlockAllocator";
};

class MIZU_RENDER_CORE_API GpuBlockAllocator : public GpuAllocator
{
  public:
    GpuBlockAllocator(const Device& device, GpuBlockAllocatorDescription desc = {});
    ~GpuBlockAllocator() override;

    std::optional<GpuAllocation> allocate(const MemoryRequirements& reqs, HeapDomain domain, GpuResourceKind kind)
        override;
    void release(const GpuAllocation& allocation) override;

    uint64_t get_num_allocations() const;
    uint64_t get_num_heaps() const;
    uint64_t get_allocated_heap_size() const;

  private:
    struct Block
    {
        std::shared_ptr<GpuHeap> heap;
        // Free ranges, offset -> size
        std::map<uint64_t, uint64_t> free_ranges;
        uint64_t used = 0;
        uint64_t pool_key = 0;
        bool dedicated = false;
    };

    GpuBlockAllocatorDescription m_description;

    mutable std::mutex m_mutex;

    uint32_t m_next_block_id = 1;
    std::unordered_map<uint32_t, Block> m_blocks;
    std::unordered_map<uint64_t, std::vector<uint32_t>> m_pools;

    uint64_t m_num_allocations = 0;
    uint64_t m_allocated_heap_size = 0;

    uint32_t create_block(
        HeapDomain domain,
        MemoryTypeId memory_type,
        uint64_t size,
        uint64_t pool_key,
        bool dedicated);
    void destroy_block(uint32_t block_id);

    static std::optional<uint64_t> find_range(const Block& block, uint64_t size, uint64_t alignment, uint64_t& waste);
    static void reserve_range(Block& block, uint64_t offset, uint64_t size);
    static void free_range(Block& block, uint64_t offset, uint64_t size);
};

} // namespace Mizu
