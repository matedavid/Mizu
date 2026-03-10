#include "dx12_buffer_resource.h"

#include "base/debug/logging.h"

#include "dx12_context.h"
#include "dx12_debug.h"
#include "dx12_device_memory_allocator.h"
#include "dx12_resource_view.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

Dx12BufferResource::Dx12BufferResource(BufferDescription desc) : m_description(std::move(desc))
{
    m_buffer_resource_description = get_dx12_resource_desc(m_description);

    if (!m_description.is_virtual)
    {
        Dx12BaseDeviceMemoryAllocator& allocator =
            dynamic_cast<Dx12BaseDeviceMemoryAllocator&>(*Dx12Context.default_device_allocator.get());

        m_allocation_info = allocator.allocate_buffer_resource(*this);

        ID3D12Heap* heap = static_cast<ID3D12Heap*>(m_allocation_info.device_memory);
        create_placed_resource(heap, m_allocation_info.offset);

        if (m_description.usage & BufferUsageBits::HostVisible)
        {
            map();
        }
    }
}

Dx12BufferResource::~Dx12BufferResource()
{
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr)
            continue;

        const Dx12BufferResourceView* internal = get_internal_buffer_resource_view(view);
        free_buffer_cpu_descriptor_handle(internal->handle);
        delete internal;
    }

    unmap();
    if (!m_description.is_virtual)
        Dx12Context.default_device_allocator->release(m_allocation_info.id);

    if (m_resource != nullptr)
        m_resource->Release();
}

ResourceView Dx12BufferResource::as_srv(const BufferResourceViewDescription& desc)
{
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

ResourceView Dx12BufferResource::as_uav(const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::UnorderedAccess,
        "Trying to create uav for buffer '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

ResourceView Dx12BufferResource::as_cbv(const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::ConstantBuffer,
        "Trying to create cbv for buffer '{}' that was not created with ConstantBuffer usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ConstantBufferView, desc);
}

ResourceView Dx12BufferResource::get_or_create_resource_view(
    ResourceViewType type,
    const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        desc.is_valid(m_description.size),
        "Trying to create resource view with invalid description for buffer '{}'",
        m_description.name);

    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr || view.view_type != type)
            continue;

        const Dx12BufferResourceView* internal_view = get_internal_buffer_resource_view(view);
        if (internal_view->offset == desc.offset && internal_view->size == desc.size)
            return view;
    }

    // Create new view
    Dx12BufferResourceView* internal = new Dx12BufferResourceView{};
    // TODO: Should enable specifying offset and size for the buffer view
    internal->offset = desc.offset;
    internal->size = desc.size;
    internal->handle = create_buffer_cpu_descriptor_handle(*this, type);

    ResourceView view{};
    view.view_type = type;
    view.internal = internal;

    m_resource_views.push_back(view);

    return view;
}

MemoryRequirements Dx12BufferResource::get_memory_requirements() const
{
    return get_dx12_buffer_memory_requirements(m_description);
}

uint8_t* Dx12BufferResource::map()
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::HostVisible,
        "Can't map buffer that does not have the HostVisible usage");
    MIZU_ASSERT(m_resource != nullptr, "Can't map buffer without a resource");

    if (m_mapped_data != nullptr)
        return m_mapped_data;

    DX12_CHECK(m_resource->Map(0, nullptr, reinterpret_cast<void**>(&m_mapped_data)));
    return m_mapped_data;
}

void Dx12BufferResource::unmap()
{
    MIZU_ASSERT(m_resource != nullptr, "Can't unmap buffer without a resource");
    if (m_mapped_data == nullptr)
        return;

    m_resource->Unmap(0, nullptr);
    m_mapped_data = nullptr;
}

void Dx12BufferResource::get_copyable_footprints(
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
    uint32_t* num_rows,
    uint64_t* row_size_in_bytes,
    uint64_t* total_size) const
{
    Dx12Context.device->handle()->GetCopyableFootprints(
        &m_buffer_resource_description, 0, 1, 0, footprints, num_rows, row_size_in_bytes, total_size);
}

void Dx12BufferResource::create_placed_resource(ID3D12Heap* heap, uint64_t offset)
{
    if (m_resource != nullptr)
    {
        MIZU_LOG_ERROR(
            "Trying to create placed resource for buffer '{}' that already has a resource. Releasing the old resource.",
            m_description.name);

        m_resource->Release();
        m_resource = nullptr;
    }

    DX12_CHECK(Dx12Context.device->handle()->CreatePlacedResource(
        heap, offset, &m_buffer_resource_description, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));

    if (!m_description.name.empty())
    {
        DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);
    }
}

D3D12_RESOURCE_DESC Dx12BufferResource::get_dx12_resource_desc(const BufferDescription& desc)
{
    uint64_t size = desc.size;
    if (desc.usage & BufferUsageBits::ConstantBuffer)
    {
        // D3D12 ConstantBufferViews must be 256 bit aligned, modifying size in here to account for this
        size = (desc.size + 255) & ~255;
    }

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = get_dx12_buffer_usage(desc.usage);

    // Use tight alignment
    resource_desc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;

    if (desc.sharing_mode == ResourceSharingMode::Concurrent)
        resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

    return resource_desc;
}

MemoryRequirements get_dx12_buffer_memory_requirements(const BufferDescription& desc)
{
    const D3D12_RESOURCE_DESC resource_desc = Dx12BufferResource::get_dx12_resource_desc(desc);

    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo(0, 1, &resource_desc);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;

    return reqs;
}

} // namespace Mizu::Dx12
