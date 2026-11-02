#include "dx12_image_resource.h"

#include <ranges>

#include "base/debug/logging.h"
#include "base/utils/hash.h"

#include "dx12_context.h"
#include "dx12_debug.h"
#include "dx12_resource_view.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

Dx12ImageResource::Dx12ImageResource(ImageDescription desc) : m_description(std::move(desc)), m_owns_resources(true)
{
    /*
    m_image_resource_description = get_dx12_resource_desc(m_description);

    if (!m_description.is_virtual)
    {
        m_allocation_info = Dx12Context.default_device_allocator->allocate_image_resource(*this);

        ID3D12Heap* heap = static_cast<ID3D12Heap*>(m_allocation_info.device_memory);
        create_placed_resource(heap, m_allocation_info.offset);
    }
    */
}

Dx12ImageResource::Dx12ImageResource(const ImageDescription& desc, const MemoryPlacement& placement)
    : m_sparse(false)
    , m_description(desc)
    , m_owns_resources(true)
{
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(m_description);

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
}

Dx12ImageResource::Dx12ImageResource(const SparseImageDescription& desc)
    : m_sparse(true)
    , m_description(desc.desc)
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
    resource_desc.Layout = resource_desc1.Layout;
    resource_desc.Flags = resource_desc.Flags;

    DX12_CHECK(Dx12Context.device->handle()->CreateReservedResource2(
        &resource_desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&m_resource)));

    DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);
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
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(m_description);
    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo2(0, 1, &resource_desc, nullptr);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;

    return reqs;
}

ImageMemoryRequirements Dx12ImageResource::get_image_memory_requirements() const
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint64_t total_size = 0;

    get_copyable_footprints(&footprint, nullptr, nullptr, &total_size);

    ImageMemoryRequirements reqs{};
    reqs.size = total_size;
    reqs.offset = footprint.Offset;
    reqs.row_pitch = footprint.Footprint.RowPitch;

    return reqs;
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

static uint32_t get_dx12_memory_type_bits(const ImageDescription& desc)
{
    const bool heap_tier2 =
        Dx12Context.device->get_feature_data_options().ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_2;

    uint32_t memory_type_bits = MEMORY_TYPE_BITS_ALL;
    if (!heap_tier2)
    {
        if (desc.usage & ImageUsageBits::Attachment)
        {
            if (is_depth_format(desc.format))
                memory_type_bits = MEMORY_TYPE_BITS_RT_DS_TEXTURE;
            else
                memory_type_bits = MEMORY_TYPE_BITS_TEXTURE;
        }
        else
        {
            memory_type_bits = MEMORY_TYPE_BITS_TEXTURE;
        }
    }

    return memory_type_bits;
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
