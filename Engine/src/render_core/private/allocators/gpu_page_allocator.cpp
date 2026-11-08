#include "render_core/allocators/gpu_page_allocator.h"

#include <format>

#include "base/debug/assert.h"

#include "render_core/rhi/device.h"

namespace Mizu
{

GpuPageAllocator::GpuPageAllocator(const Device& device, GpuPageAllocatorDescription desc)
    : m_device(device)
    , m_description(std::move(desc))
{
    MIZU_ASSERT(m_description.page_size > 0, "Page size can't be 0");
    MIZU_ASSERT(m_description.pages_per_heap > 0, "Pages per heap can't be 0");
}

GpuPageAllocator::~GpuPageAllocator()
{
    MIZU_ASSERT(
        m_num_allocated_pages == 0,
        "GpuPageAllocator '{}' destroyed with {} pages still allocated",
        m_description.name,
        m_num_allocated_pages);
}

std::optional<GpuPage> GpuPageAllocator::allocate_page()
{
    std::lock_guard lock(m_mutex);

    if (m_free_pages.empty())
    {
        if (!grow())
            return std::nullopt;
    }

    const uint32_t index = m_free_pages.back();
    m_free_pages.pop_back();
    m_num_allocated_pages += 1;

    return make_page(index);
}

bool GpuPageAllocator::allocate_pages(uint32_t count, std::vector<GpuPage>& out_pages)
{
    std::lock_guard lock(m_mutex);

    while (m_free_pages.size() < count)
    {
        if (!grow())
            return false;
    }

    out_pages.reserve(out_pages.size() + count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t index = m_free_pages.back();
        m_free_pages.pop_back();

        out_pages.push_back(make_page(index));
    }

    m_num_allocated_pages += count;

    return true;
}

void GpuPageAllocator::free_page(const GpuPage& page)
{
    free_pages(std::span(&page, 1));
}

void GpuPageAllocator::free_pages(std::span<const GpuPage> pages)
{
    std::lock_guard lock(m_mutex);

    for (const GpuPage& page : pages)
    {
        MIZU_ASSERT(
            page.index < m_heaps.size() * m_description.pages_per_heap
                && m_heaps[page.index / m_description.pages_per_heap].get() == page.placement.heap,
            "Freeing a page that does not belong to '{}'",
            m_description.name);
        MIZU_ASSERT(m_num_allocated_pages > 0, "Freeing more pages than allocated");

        m_free_pages.push_back(page.index);
        m_num_allocated_pages -= 1;
    }
}

uint32_t GpuPageAllocator::get_num_allocated_pages() const
{
    std::lock_guard lock(m_mutex);
    return m_num_allocated_pages;
}

uint32_t GpuPageAllocator::get_num_heaps() const
{
    std::lock_guard lock(m_mutex);
    return static_cast<uint32_t>(m_heaps.size());
}

bool GpuPageAllocator::grow()
{
    if (m_description.max_heaps != 0 && m_heaps.size() >= m_description.max_heaps)
        return false;

    const uint32_t heap_index = static_cast<uint32_t>(m_heaps.size());

    GpuHeapDescription heap_desc{};
    heap_desc.size = m_description.page_size * m_description.pages_per_heap;
    heap_desc.domain = m_description.domain;
    heap_desc.memory_type = m_description.memory_type;
    heap_desc.name = std::format("{}_Heap_{}", m_description.name, heap_index);

    m_heaps.push_back(m_device.create_gpu_heap(heap_desc));

    // Pushed in reverse so pages are handed out in increasing order
    const uint32_t first_page = heap_index * m_description.pages_per_heap;
    for (uint32_t i = m_description.pages_per_heap; i > 0; --i)
    {
        m_free_pages.push_back(first_page + i - 1);
    }

    return true;
}

GpuPage GpuPageAllocator::make_page(uint32_t index) const
{
    const uint32_t heap_index = index / m_description.pages_per_heap;
    const uint32_t page_in_heap = index % m_description.pages_per_heap;

    return GpuPage{
        .placement =
            MemoryPlacement{
                .heap = m_heaps[heap_index].get(),
                .offset = static_cast<uint64_t>(page_in_heap) * m_description.page_size,
            },
        .index = index,
    };
}

} // namespace Mizu
