#pragma once

#include "base/containers/inplace_vector.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/device_memory_allocator.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12BufferResource : public BufferResource
{
  public:
    Dx12BufferResource(BufferDescription desc);
    ~Dx12BufferResource();

    ResourceView as_srv(const BufferResourceViewDescription& desc) override;
    ResourceView as_uav(const BufferResourceViewDescription& desc) override;
    ResourceView as_cbv(const BufferResourceViewDescription& desc) override;

    MemoryRequirements get_memory_requirements() const override;

    uint8_t* get_mapped_data() const override { return m_mapped_data; }
    uint8_t* map() override;
    void unmap() override;

    uint64_t get_size() const override { return m_description.size; }
    uint64_t get_stride() const override { return m_description.stride; }
    BufferUsageBits get_usage() const override { return m_description.usage; }
    ResourceSharingMode get_sharing_mode() const override { return m_description.sharing_mode; }

    const std::string& get_name() const override { return m_description.name; }

    void get_copyable_footprints(
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
        uint32_t* num_rows,
        uint64_t* row_size_in_bytes,
        uint64_t* total_size) const;

    void create_placed_resource(ID3D12Heap* heap, uint64_t offset);
    D3D12_RESOURCE_DESC get_resource_description() const { return m_buffer_resource_description; }

    static D3D12_RESOURCE_DESC get_dx12_resource_desc(const BufferDescription& desc);

    ID3D12Resource* handle() const { return m_resource; }
    D3D12_GPU_VIRTUAL_ADDRESS get_gpu_address() const { return m_resource->GetGPUVirtualAddress(); }

    const AllocationInfo& get_allocation_info() const { return m_allocation_info; }

  private:
    ID3D12Resource* m_resource = nullptr;
    D3D12_RESOURCE_DESC m_buffer_resource_description{};
    uint8_t* m_mapped_data = nullptr;

    // Considering 2 srvs, 2 uavs and 2 cbvs at max
    static constexpr size_t MAX_RESOURCE_VIEWS = 6;
    inplace_vector<ResourceView, MAX_RESOURCE_VIEWS> m_resource_views;

    ResourceView get_or_create_resource_view(ResourceViewType type, const BufferResourceViewDescription& desc);

    BufferDescription m_description{};
    AllocationInfo m_allocation_info{};
};

MemoryRequirements get_dx12_buffer_memory_requirements(const BufferDescription& desc);

} // namespace Mizu::Dx12