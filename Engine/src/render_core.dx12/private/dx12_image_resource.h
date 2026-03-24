#pragma once

#include <unordered_map>

#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/image_resource.h"

#include "dx12_core.h"
#include "dx12_resource_view.h"

namespace Mizu::Dx12
{

class Dx12ImageResource : public ImageResource
{
  public:
    Dx12ImageResource(ImageDescription desc);
    // Only used by Dx12Swapchain
    Dx12ImageResource(
        uint32_t width,
        uint32_t height,
        ImageFormat format,
        ImageUsageBits usage,
        ID3D12Resource* image,
        bool owns_resources);
    ~Dx12ImageResource();

    Dx12ImageResourceView as_rtv(const ImageResourceViewDescription& desc);

    MemoryRequirements get_memory_requirements() const override;
    ImageMemoryRequirements get_image_memory_requirements() const override;

    uint32_t get_width() const override { return m_description.width; }
    uint32_t get_height() const override { return m_description.height; }
    uint32_t get_depth() const override { return m_description.depth; }
    ImageType get_image_type() const override { return m_description.type; }
    ImageFormat get_format() const override { return m_description.format; }
    ImageUsageBits get_usage() const override { return m_description.usage; }
    ResourceSharingMode get_sharing_mode() const override { return m_description.sharing_mode; }
    uint32_t get_num_mips() const override { return m_description.num_mips; }
    uint32_t get_num_layers() const override { return m_description.num_layers; }

    std::string_view get_name() const override { return m_description.name; }

    void get_copyable_footprints(
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
        uint32_t* num_rows,
        uint64_t* row_size_in_bytes,
        uint64_t* total_size) const;

    void create_placed_resource(ID3D12Heap* heap, uint64_t offset);
    D3D12_RESOURCE_DESC get_resource_description() const { return m_image_resource_description; }

    static D3D12_RESOURCE_DESC get_dx12_resource_desc(const ImageDescription& desc);

    ID3D12Resource* handle() const { return m_resource; }

    void set_allocation_info(const AllocationInfo& info) { m_allocation_info = info; }
    const AllocationInfo& get_allocation_info() const { return m_allocation_info; }

  private:
    ID3D12Resource* m_resource = nullptr;
    D3D12_RESOURCE_DESC m_image_resource_description{};

    std::unordered_map<size_t, Dx12ImageResourceView> m_resource_views;

    ImageDescription m_description;
    AllocationInfo m_allocation_info{};

    bool m_owns_resources = true;
};

MemoryRequirements get_dx12_image_memory_requirements(const ImageDescription& desc);

} // namespace Mizu::Dx12
