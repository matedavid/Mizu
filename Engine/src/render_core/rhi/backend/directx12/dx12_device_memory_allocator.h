#pragma once

#include <unordered_map>

#include "render_core/rhi/device_memory_allocator.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

// Forward declarations
class Dx12BufferResource;

class Dx12BaseDeviceMemoryAllocator : public BaseDeviceMemoryAllocator
{
  public:
    Dx12BaseDeviceMemoryAllocator() = default;
    ~Dx12BaseDeviceMemoryAllocator() override;

    AllocationInfo allocate_buffer_resource(const BufferResource& buffer) override;
    AllocationInfo allocate_image_resource(const ImageResource& image) override;

    uint8_t* get_mapped_memory(AllocationId id) const override;

    void release(AllocationId id) override;
    void reset() override;

    void map_memory_if_host_visible(const Dx12BufferResource& buffer, AllocationId id);

  private:
    std::unordered_map<AllocationId, ID3D12Heap*> m_memory_allocations;
    std::unordered_map<AllocationId, uint8_t*> m_mapped_allocations;
};

} // namespace Mizu::Dx12