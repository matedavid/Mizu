#include "render_core/rhi/gpu_heap.h"

#include "base/debug/assert.h"

namespace Mizu
{

void validate_placement(
    const MemoryPlacement& placement,
    const MemoryRequirements& reqs,
    std::string_view resource_name)
{
    MIZU_ASSERT(placement.heap != nullptr, "Trying to place resource '{}' without a heap", resource_name);
    MIZU_ASSERT(
        reqs.alignment == 0 || placement.offset % reqs.alignment == 0,
        "Placement offset {} of resource '{}' is not aligned to {}",
        placement.offset,
        resource_name,
        reqs.alignment);
    MIZU_ASSERT(
        placement.offset + reqs.size <= placement.heap->get_size(),
        "Resource '{}' range [{}, {}) exceeds heap '{}' size ({})",
        resource_name,
        placement.offset,
        placement.offset + reqs.size,
        placement.heap->get_name(),
        placement.heap->get_size());
    MIZU_ASSERT(
        placement.heap->is_compatible(reqs),
        "Resource '{}' is not compatible with heap '{}'",
        resource_name,
        placement.heap->get_name());
}

} // namespace Mizu
