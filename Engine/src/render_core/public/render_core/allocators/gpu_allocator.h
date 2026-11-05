#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "mizu_render_core_module.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/gpu_heap.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/memory.h"

namespace Mizu
{

class Device;

enum class GpuResourceKind
{
    Buffer,
    Image,
};

struct GpuAllocation
{
    MemoryPlacement placement{};
    uint64_t size = 0;
    uint64_t handle = 0;
};

class MIZU_RENDER_CORE_API GpuAllocator
{
  public:
    explicit GpuAllocator(const Device& device) : m_device(device) {}
    virtual ~GpuAllocator() = default;

    GpuAllocator(const GpuAllocator&) = delete;
    GpuAllocator& operator=(const GpuAllocator&) = delete;

    virtual std::optional<GpuAllocation> allocate(
        const MemoryRequirements& reqs,
        HeapDomain domain,
        GpuResourceKind kind) = 0;
    virtual void release(const GpuAllocation& allocation) = 0;

    std::shared_ptr<BufferResource> create_buffer(
        const BufferDescription& desc,
        HeapDomain domain = HeapDomain::DeviceLocal);
    std::shared_ptr<ImageResource> create_image(
        const ImageDescription& desc,
        HeapDomain domain = HeapDomain::DeviceLocal);

  protected:
    const Device& m_device;
};

} // namespace Mizu
