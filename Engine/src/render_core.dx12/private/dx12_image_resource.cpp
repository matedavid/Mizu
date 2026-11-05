#include "dx12_image_resource.h"

#include <glm/glm.hpp>
#include <ranges>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/utils/hash.h"

#include "dx12_context.h"
#include "dx12_debug.h"
#include "dx12_resource_view.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

namespace
{

uint32_t get_dx12_memory_type_bits(const ImageDescription& desc)
{
    const bool heap_tier2 =
        Dx12Context.device->get_feature_data_options().ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_2;

    if (heap_tier2)
        return MEMORY_TYPE_BITS_ALL;

    return (desc.usage & ImageUsageBits::Attachment) ? MEMORY_TYPE_BITS_RT_DS_TEXTURE : MEMORY_TYPE_BITS_TEXTURE;
}

} // namespace

Dx12ImageResource::Dx12ImageResource(const ImageDescription& desc, const MemoryPlacement& placement)
    : m_sparse(false)
    , m_description(desc)
    , m_owns_resources(true)
{
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(m_description);

    validate_placement(placement, get_dx12_image_memory_requirements(m_description), m_description.name);

    const Dx12GpuHeap& native_heap = static_cast<const Dx12GpuHeap&>(*placement.heap);
    DX12_CHECK(Dx12Context.device->handle()->CreatePlacedResource2(
        native_heap.handle(),
        placement.offset,
        &resource_desc,
        D3D12_BARRIER_LAYOUT_UNDEFINED,
        nullptr,
        0,
        nullptr,
        IID_PPV_ARGS(&m_resource)));

    DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);

    init_memory_requirements();
}

Dx12ImageResource::Dx12ImageResource(const ImageDescription& desc, SparseTag)
    : m_sparse(true)
    , m_description(desc)
    , m_owns_resources(true)
{
    const D3D12_RESOURCE_DESC1 resource_desc1 = get_dx12_resource_desc(m_description);

    // TODO: Not the happiest person right now
    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = resource_desc1.Dimension;
    resource_desc.Alignment = resource_desc1.Alignment;
    resource_desc.Width = resource_desc1.Width;
    resource_desc.Height = resource_desc1.Height;
    resource_desc.DepthOrArraySize = resource_desc1.DepthOrArraySize;
    resource_desc.MipLevels = resource_desc1.MipLevels;
    resource_desc.Format = resource_desc1.Format;
    resource_desc.SampleDesc = resource_desc1.SampleDesc;
    // Reserved textures must use the 64 KiB tiled layout, and tight alignment is not valid for them
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    resource_desc.Flags = resource_desc1.Flags & ~D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;

    DX12_CHECK(Dx12Context.device->handle()->CreateReservedResource2(
        &resource_desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&m_resource)));

    DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);

    init_memory_requirements();
}

Dx12ImageResource::Dx12ImageResource(
    uint32_t width,
    uint32_t height,
    ImageFormat format,
    ImageUsageBits usage,
    ID3D12Resource* image,
    bool owns_resources)
{
    m_description.width = width;
    m_description.height = height;
    m_description.format = format;
    m_description.usage = usage;

    m_owns_resources = owns_resources;

    m_resource = image;
}

Dx12ImageResource::~Dx12ImageResource()
{
    for (const Dx12ImageResourceView& view : m_resource_views | std::views::values)
    {
        MIZU_ASSERT(
            view.type == ResourceViewType::RenderTargetView,
            "Only RTV/DSV image views are cached on Dx12ImageResource");

        if (is_depth_format(view.format))
            Dx12Context.dsv_heap->free(view.handle);
        else
            Dx12Context.rtv_heap->free(view.handle);
    }

    if (m_owns_resources)
    {
        m_resource->Release();
    }
}

Dx12ImageResourceView Dx12ImageResource::as_rtv(const ImageResourceViewDescription& desc, bool read_only)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Attachment,
        "Trying to create rtv for image '{}' that was not created with Attachment usage",
        m_description.name);
    MIZU_ASSERT(
        desc.is_valid(m_description.num_mips, m_description.num_layers),
        "Trying to create resource view with invalid description for image '{}'",
        m_description.name);

    const size_t hash = hash_compute(desc.hash(), ResourceViewType::RenderTargetView, read_only);

    const auto it = m_resource_views.find(hash);
    if (it != m_resource_views.end())
        return it->second;

    Dx12ImageResourceView resource_view;
    resource_view.description = desc;
    resource_view.format = desc.override_format.value_or(m_description.format);
    resource_view.type = ResourceViewType::RenderTargetView;

    if (is_depth_format(resource_view.format))
    {
        resource_view.handle = Dx12Context.dsv_heap->allocate();
        create_image_dsv(*this, desc, read_only, resource_view.handle);
    }
    else
    {
        resource_view.handle = Dx12Context.rtv_heap->allocate();
        create_image_rtv(*this, desc, resource_view.handle);
    }

    m_resource_views.emplace(hash, resource_view);

    return resource_view;
}

MemoryRequirements Dx12ImageResource::get_memory_requirements() const
{
    return m_memory_requirements;
}

ImageMemoryRequirements Dx12ImageResource::get_image_memory_requirements() const
{
    return m_image_memory_requirements;
}

SparseImageMemoryRequirements Dx12ImageResource::get_sparse_memory_requirements() const
{
    MIZU_ASSERT(m_sparse, "Trying to get sparse memory requirements of non sparse image '{}'", m_description.name);
    return m_sparse_requirements;
}

const D3D12_SUBRESOURCE_TILING& Dx12ImageResource::get_subresource_tiling(uint32_t subresource) const
{
    MIZU_ASSERT(m_sparse, "Trying to get subresource tiling of non sparse image '{}'", m_description.name);
    MIZU_ASSERT(subresource < m_subresource_tilings.size(), "Invalid subresource index {}", subresource);

    const D3D12_SUBRESOURCE_TILING& tiling = m_subresource_tilings[subresource];
    MIZU_ASSERT(
        tiling.StartTileIndexInOverallResource != D3D12_PACKED_TILE,
        "Subresource {} of image '{}' is part of the packed mips",
        subresource,
        m_description.name);

    return tiling;
}

void Dx12ImageResource::get_copyable_footprints(
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
    uint32_t* num_rows,
    uint64_t* row_size_in_bytes,
    uint64_t* total_size,
    uint32_t first_subresource,
    uint32_t num_subresources) const
{
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(m_description);
    Dx12Context.device->handle()->GetCopyableFootprints1(
        &resource_desc, first_subresource, num_subresources, 0, footprints, num_rows, row_size_in_bytes, total_size);
}

void Dx12ImageResource::init_memory_requirements()
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint64_t total_size = 0;
    get_copyable_footprints(&footprint, nullptr, nullptr, &total_size);

    m_image_memory_requirements = ImageMemoryRequirements{
        .size = total_size,
        .offset = footprint.Offset,
        .row_pitch = footprint.Footprint.RowPitch,
    };

    if (!m_sparse)
    {
        m_memory_requirements = get_dx12_image_memory_requirements(m_description);
        return;
    }

    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(m_description);
    const UINT array_size = resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
                                ? 1
                                : static_cast<UINT>(resource_desc.DepthOrArraySize);

    UINT num_subresource_tilings = static_cast<UINT>(resource_desc.MipLevels) * array_size;
    m_subresource_tilings.resize(num_subresource_tilings);

    UINT num_tiles = 0;
    Dx12Context.device->handle()->GetResourceTiling(
        m_resource,
        &num_tiles,
        &m_packed_mip_info,
        &m_tile_shape,
        &num_subresource_tilings,
        0,
        m_subresource_tilings.data());

    constexpr uint64_t TILE_SIZE = D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;

    m_memory_requirements = MemoryRequirements{
        .size = static_cast<uint64_t>(num_tiles) * TILE_SIZE,
        .alignment = TILE_SIZE,
        .memory_type_bits = get_dx12_memory_type_bits(m_description),
    };

    m_sparse_requirements.memory = m_memory_requirements;
    m_sparse_requirements.page_size = TILE_SIZE;
    m_sparse_requirements.tile_extent =
        glm::uvec3{m_tile_shape.WidthInTexels, m_tile_shape.HeightInTexels, m_tile_shape.DepthInTexels};
    m_sparse_requirements.mip_tail_first_mip = m_packed_mip_info.NumStandardMips;
    m_sparse_requirements.mip_tail_size = static_cast<uint64_t>(m_packed_mip_info.NumTilesForPackedMips) * TILE_SIZE;
    m_sparse_requirements.single_mip_tail = false;
}

D3D12_RESOURCE_DESC1 get_dx12_resource_desc(const ImageDescription& desc)
{
    D3D12_RESOURCE_DESC1 resource_desc{};
    resource_desc.Dimension = get_dx12_image_type(desc.type);
    resource_desc.Alignment = 0;
    resource_desc.Width = desc.width;
    resource_desc.Height = desc.height;
    resource_desc.DepthOrArraySize = static_cast<uint16_t>(desc.depth);
    resource_desc.MipLevels = static_cast<uint16_t>(desc.num_mips);
    resource_desc.Format = get_dx12_image_format(desc.format);
    resource_desc.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = get_dx12_usage(desc.usage, desc.format);

    // Use tight alignment
    resource_desc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;

    if (desc.sharing_mode == ResourceSharingMode::Concurrent && !is_depth_format(desc.format))
        resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

    return resource_desc;
}

MemoryRequirements get_dx12_image_memory_requirements(const ImageDescription& desc)
{
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(desc);

    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo2(0, 1, &resource_desc, nullptr);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;
    reqs.memory_type_bits = get_dx12_memory_type_bits(desc);

    return reqs;
}

} // namespace Mizu::Dx12
