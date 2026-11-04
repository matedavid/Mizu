#pragma once

#include <unordered_map>
#include <vector>

#include "render_core/rhi/image_resource.h"

#include "dx12_core.h"
#include "dx12_gpu_heap.h"
#include "dx12_resource_view.h"

namespace Mizu::Dx12
{

class Dx12ImageResource : public ImageResource
{
  public:
    Dx12ImageResource(ImageDescription desc);
    Dx12ImageResource(const ImageDescription& desc, const MemoryPlacement& placement);
    Dx12ImageResource(const SparseImageDescription& desc);
    // Only used by Dx12Swapchain
    Dx12ImageResource(
        uint32_t width,
        uint32_t height,
        ImageFormat format,
        ImageUsageBits usage,
        ID3D12Resource* image,
        bool owns_resources);
    ~Dx12ImageResource() override;

    Dx12ImageResourceView as_rtv(const ImageResourceViewDescription& desc, bool read_only = false);

    MemoryRequirements get_memory_requirements() const override;
    ImageMemoryRequirements get_image_memory_requirements() const override;
    SparseImageMemoryRequirements get_sparse_memory_requirements() const override;

    const ImageDescription& get_description() const override { return m_description; }
    bool is_sparse() const override { return m_sparse; }

    const D3D12_PACKED_MIP_INFO& get_packed_mip_info() const { return m_packed_mip_info; }
    const D3D12_TILE_SHAPE& get_tile_shape() const { return m_tile_shape; }
    const D3D12_SUBRESOURCE_TILING& get_subresource_tiling(uint32_t subresource) const;

    void get_copyable_footprints(
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
        uint32_t* num_rows,
        uint64_t* row_size_in_bytes,
        uint64_t* total_size,
        uint32_t first_subresource = 0,
        uint32_t num_subresources = 1) const;

    ID3D12Resource* handle() const { return m_resource; }

  private:
    ID3D12Resource* m_resource = nullptr;
    bool m_sparse = false;

    MemoryRequirements m_memory_requirements{};
    ImageMemoryRequirements m_image_memory_requirements{};

    SparseImageMemoryRequirements m_sparse_requirements{};
    D3D12_PACKED_MIP_INFO m_packed_mip_info{};
    D3D12_TILE_SHAPE m_tile_shape{};
    std::vector<D3D12_SUBRESOURCE_TILING> m_subresource_tilings{};

    ImageDescription m_description{};
    bool m_owns_resources = true;

    std::unordered_map<size_t, Dx12ImageResourceView> m_resource_views;

    void init_memory_requirements();
};

D3D12_RESOURCE_DESC1 get_dx12_resource_desc(const ImageDescription& desc);
MemoryRequirements get_dx12_image_memory_requirements(const ImageDescription& desc);

} // namespace Mizu::Dx12
