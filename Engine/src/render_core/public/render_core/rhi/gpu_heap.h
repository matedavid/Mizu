#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "mizu_render_core_module.h"
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
    MemoryTypeId memory_type = 0;

    std::string name{};
};

class GpuHeap
{
  public:
    virtual ~GpuHeap() = default;

    virtual bool is_compatible(const MemoryRequirements& reqs) const = 0;
    virtual const GpuHeapDescription& get_description() const = 0;

    uint64_t get_size() const { return get_description().size; }
    HeapDomain get_domain() const { return get_description().domain; }
    MemoryTypeId get_memory_type() const { return get_description().memory_type; }
    bool is_host_visible() const { return get_domain() != HeapDomain::DeviceLocal; }
    std::string_view get_name() const { return get_description().name; }
};

struct MemoryPlacement
{
    GpuHeap* heap = nullptr;
    uint64_t offset = 0;
};

MIZU_RENDER_CORE_API void validate_placement(
    const MemoryPlacement& placement,
    const MemoryRequirements& reqs,
    std::string_view resource_name);

} // namespace Mizu
