#include "dx12_image_resource.h"

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
    m_image_resource_description = get_dx12_resource_desc(m_description);

    if (!m_description.is_virtual)
    {
        m_allocation_info = Dx12Context.default_device_allocator->allocate_image_resource(*this);

        ID3D12Heap* heap = static_cast<ID3D12Heap*>(m_allocation_info.device_memory);
        create_placed_resource(heap, m_allocation_info.offset);
    }
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
    for (const auto& [_, view] : m_resource_views)
    {
        free_image_cpu_descriptor_handle(view.handle, view.type, view.format);
    }

    if (m_owns_resources)
    {
        if (!m_description.is_virtual)
        {
            Dx12Context.default_device_allocator->release(m_allocation_info.id);
        }

        m_resource->Release();
    }
}

Dx12ImageResourceView Dx12ImageResource::as_srv(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Sampled,
        "Trying to create srv for image '{}' that was not created with Sampled usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

Dx12ImageResourceView Dx12ImageResource::as_uav(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::UnorderedAccess,
        "Trying to create uav for image '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

Dx12ImageResourceView Dx12ImageResource::as_rtv(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Attachment,
        "Trying to create rtv for image '{}' that was not created with Attachment usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::RenderTargetView, desc);
}

Dx12ImageResourceView Dx12ImageResource::get_or_create_resource_view(
    ResourceViewType type,
    const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        desc.is_valid(m_description.num_mips, m_description.num_layers),
        "Trying to create resource view with invalid description for image '{}'",
        m_description.name);

    const size_t hash = hash_compute(desc.hash(), type);

    const auto it = m_resource_views.find(hash);
    if (it != m_resource_views.end())
        return it->second;

    Dx12ImageResourceView resource_view;
    resource_view.description = desc;
    resource_view.format = desc.override_format.value_or(m_description.format);
    resource_view.type = type;
    resource_view.handle = create_image_cpu_descriptor_handle(*this, type, desc);

    m_resource_views.emplace(hash, resource_view);

    return resource_view;
}

MemoryRequirements Dx12ImageResource::get_memory_requirements() const
{
    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo(0, 1, &m_image_resource_description);

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
    uint64_t* total_size) const
{
    Dx12Context.device->handle()->GetCopyableFootprints(
        &m_image_resource_description, 0, 1, 0, footprints, num_rows, row_size_in_bytes, total_size);
}

void Dx12ImageResource::create_placed_resource(ID3D12Heap* heap, uint64_t offset)
{
    if (m_resource != nullptr)
    {
        MIZU_LOG_ERROR(
            "Trying to create placed resource for image '{}' that already has a resource. Releasing the old resource.",
            m_description.name);

        m_resource->Release();
        m_resource = nullptr;
    }

    DX12_CHECK(Dx12Context.device->handle()->CreatePlacedResource(
        heap, offset, &m_image_resource_description, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));

    if (!m_description.name.empty())
    {
        DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);
    }
}

D3D12_RESOURCE_DESC Dx12ImageResource::get_dx12_resource_desc(const ImageDescription& desc)
{
    D3D12_RESOURCE_DESC resource_desc{};
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

    if (desc.sharing_mode == ResourceSharingMode::Concurrent)
        resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

    return resource_desc;
}

MemoryRequirements get_dx12_image_memory_requirements(const ImageDescription& desc)
{
    const D3D12_RESOURCE_DESC resource_desc = Dx12ImageResource::get_dx12_resource_desc(desc);

    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo(0, 1, &resource_desc);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;

    return reqs;
}

} // namespace Mizu::Dx12
