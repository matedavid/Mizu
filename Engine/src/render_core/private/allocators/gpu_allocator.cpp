#include "render_core/allocators/gpu_allocator.h"

#include "base/debug/assert.h"

#include "render_core/rhi/device.h"

namespace Mizu
{

namespace
{

template <typename ResourceT>
struct AllocatedResourceDeleter
{
    std::shared_ptr<ResourceT> resource;
    GpuAllocator* allocator;
    GpuAllocation allocation;

    void operator()(ResourceT*)
    {
        resource.reset();
        allocator->release(allocation);
    }
};

template <typename ResourceT>
std::shared_ptr<ResourceT> wrap_allocated(
    std::shared_ptr<ResourceT> resource,
    GpuAllocator& allocator,
    const GpuAllocation& allocation)
{
    ResourceT* raw = resource.get();
    return std::shared_ptr<ResourceT>(
        raw, AllocatedResourceDeleter<ResourceT>{std::move(resource), &allocator, allocation});
}

} // namespace

std::shared_ptr<BufferResource> GpuAllocator::create_buffer(const BufferDescription& desc, HeapDomain domain)
{
    const MemoryRequirements reqs = m_device.get_buffer_memory_requirements(desc);

    const std::optional<GpuAllocation> allocation = allocate(reqs, domain, GpuResourceKind::Buffer);
    MIZU_ASSERT(allocation.has_value(), "Failed to allocate memory for buffer '{}'", desc.name);

    return wrap_allocated(m_device.create_buffer(desc, allocation->placement), *this, *allocation);
}

std::shared_ptr<ImageResource> GpuAllocator::create_image(const ImageDescription& desc, HeapDomain domain)
{
    const MemoryRequirements reqs = m_device.get_image_memory_requirements(desc);

    const std::optional<GpuAllocation> allocation = allocate(reqs, domain, GpuResourceKind::Image);
    MIZU_ASSERT(allocation.has_value(), "Failed to allocate memory for image '{}'", desc.name);

    return wrap_allocated(m_device.create_image(desc, allocation->placement), *this, *allocation);
}

} // namespace Mizu
