#include "render/render_graph/render_graph_transient_memory.h"

#include <algorithm>

#include "base/math/math.h"
#include "render_core/rhi/device.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

static constexpr uint64_t TRANSIENT_HEAP_SIZE_ALIGNMENT = 64ull * 1024; // 64 KiB

RenderGraphTransientMemory::RenderGraphTransientMemory(std::string name) : m_name(std::move(name)) {}

bool RenderGraphTransientMemory::needs_recreate(MemoryTypeId memory_type, uint64_t size) const
{
    return m_heap == nullptr || m_heap->get_memory_type() != memory_type || m_heap->get_size() < size;
}

GpuHeap& RenderGraphTransientMemory::recreate(MemoryTypeId memory_type, uint64_t size)
{
    uint64_t new_size = size;
    if (m_heap != nullptr && m_heap->get_memory_type() == memory_type)
        new_size = std::max(size, m_heap->get_size() + m_heap->get_size() / 2);

    GpuHeapDescription heap_desc{};
    heap_desc.size = math::align_up(new_size, TRANSIENT_HEAP_SIZE_ALIGNMENT);
    heap_desc.domain = HeapDomain::DeviceLocal;
    heap_desc.memory_type = memory_type;
    heap_desc.name = m_name;

    m_heap = g_render_device->create_gpu_heap(heap_desc);

    return *m_heap;
}

} // namespace Mizu
