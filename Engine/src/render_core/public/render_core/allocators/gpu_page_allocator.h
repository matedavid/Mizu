#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "mizu_render_core_module.h"
#include "render_core/rhi/gpu_heap.h"
#include "render_core/rhi/memory.h"

namespace Mizu
{

// Forward declarations
class Device;

struct GpuPage
{
    MemoryPlacement placement{};
    uint32_t index = 0;
};

struct GpuPageAllocatorDescription
{
    HeapDomain domain = HeapDomain::DeviceLocal;
    MemoryTypeId memory_type = 0;
    uint64_t page_size = 64 * 1024;
    uint32_t pages_per_heap = 256;
    // 0 = unlimited
    uint32_t max_heaps = 0;

    std::string name = "GpuPageAllocator";
};

class MIZU_RENDER_CORE_API GpuPageAllocator
{
  public:
    GpuPageAllocator(const Device& device, GpuPageAllocatorDescription desc);
    ~GpuPageAllocator();

    GpuPageAllocator(const GpuPageAllocator&) = delete;
    GpuPageAllocator& operator=(const GpuPageAllocator&) = delete;

    std::optional<GpuPage> allocate_page();
    bool allocate_pages(uint32_t count, std::vector<GpuPage>& out_pages);

    void free_page(const GpuPage& page);
    void free_pages(std::span<const GpuPage> pages);

    uint64_t get_page_size() const { return m_description.page_size; }
    uint32_t get_num_allocated_pages() const;
    uint32_t get_num_heaps() const;

  private:
    const Device& m_device;
    GpuPageAllocatorDescription m_description;

    mutable std::mutex m_mutex;

    std::vector<std::shared_ptr<GpuHeap>> m_heaps;
    std::vector<uint32_t> m_free_pages;
    uint32_t m_num_allocated_pages = 0;

    bool grow();
    GpuPage make_page(uint32_t index) const;
};

} // namespace Mizu
