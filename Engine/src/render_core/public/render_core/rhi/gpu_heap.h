#pragma once

#include <cstdint>
#include <string>

#include "render_core/rhi/memory.h"

namespace Mizu
{

enum class HeapDomain
{
    DeviceLocal,
    Upload,
    Readback,
};

struct GpuHeapDescription
{
    uint64_t size = 0;
    HeapDomain domain = HeapDomain::DeviceLocal;
    uint32_t memory_type_bits = 0;

    std::string name{};
};

class GpuHeap
{
  public:
    virtual ~GpuHeap() = default;

    virtual bool is_compatible(const MemoryRequirements& reqs) const = 0;
    virtual const GpuHeapDescription& get_description() const = 0;
    uint64_t get_size() const { return get_description().size; }

    bool is_host_visible() const { return get_description().domain == HeapDomain::Upload; }
};

struct MemoryPlacement
{
    // MemoryPlacement(GpuHeap& heap_, uint64_t offset_, uint64_t size_) : heap(heap_), offset(offset_), size(size_) {}

    GpuHeap* heap = nullptr;
    uint64_t offset = 0;
    uint64_t size = 0;
};

} // namespace Mizu
