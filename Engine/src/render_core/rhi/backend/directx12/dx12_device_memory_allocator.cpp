#include "dx12_device_memory_allocator.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"

namespace Mizu::Dx12
{

//
// Dx12BaseDeviceMemoryAllocator
//

Dx12BaseDeviceMemoryAllocator::~Dx12BaseDeviceMemoryAllocator()
{
#if MIZU_DEBUG
    if (!m_memory_allocations.empty())
    {
        MIZU_LOG_ERROR(
            "Some memory chunks were not released manually ({}), this could cause problems",
            m_memory_allocations.size());
    }
#endif

    reset();
}

AllocationInfo Dx12BaseDeviceMemoryAllocator::allocate_buffer_resource(const BufferResource& buffer)
{
    const MemoryRequirements memory_requirements = buffer.get_memory_requirements();

    D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
    if (buffer.get_usage() & BufferUsageBits::HostVisible)
    {
        heap_type = D3D12_HEAP_TYPE_UPLOAD;
    }

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = heap_type;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 0;
    heap_properties.VisibleNodeMask = 0;

    D3D12_HEAP_DESC heap_desc{};
    heap_desc.SizeInBytes = memory_requirements.size;
    heap_desc.Properties = heap_properties;
    heap_desc.Alignment = memory_requirements.alignment;
    heap_desc.Flags = D3D12_HEAP_FLAG_NONE;

    ID3D12Heap* heap;
    Dx12Context.device->handle()->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap));

    AllocationInfo info{};
    info.id = AllocationId();
    info.size = memory_requirements.size;
    info.offset = 0;
    info.device_memory = (void*)heap;

    m_memory_allocations.insert({info.id, heap});

    return info;
}

AllocationInfo Dx12BaseDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image)
{
    const MemoryRequirements memory_requirements = image.get_memory_requirements();

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 0;
    heap_properties.VisibleNodeMask = 0;

    D3D12_HEAP_DESC heap_desc{};
    heap_desc.SizeInBytes = memory_requirements.size;
    heap_desc.Properties = heap_properties;
    heap_desc.Alignment = memory_requirements.alignment;
    heap_desc.Flags = D3D12_HEAP_FLAG_NONE;

    ID3D12Heap* heap;
    Dx12Context.device->handle()->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap));

    AllocationInfo info{};
    info.id = AllocationId();
    info.size = memory_requirements.size;
    info.offset = 0;
    info.device_memory = (void*)heap;

    m_memory_allocations.insert({info.id, heap});

    return info;
}

uint8_t* Dx12BaseDeviceMemoryAllocator::get_mapped_memory(AllocationId id) const
{
    const auto it = m_mapped_allocations.find(id);
    MIZU_ASSERT(
        it != m_mapped_allocations.end(),
        "Could not find allocation with id {} that is mapped",
        static_cast<UUID::Type>(id));

    return it->second;
}

void Dx12BaseDeviceMemoryAllocator::release(AllocationId id)
{
    const auto it = m_memory_allocations.find(id);
    if (it == m_memory_allocations.end())
    {
        MIZU_LOG_WARNING("Allocation {} does not exist", static_cast<UUID::Type>(id));
        return;
    }

    it->second->Release();
    m_memory_allocations.erase(it);

    const auto mapped_it = m_mapped_allocations.find(id);
    if (mapped_it != m_mapped_allocations.end())
    {
        m_mapped_allocations.erase(mapped_it);
    }
}

void Dx12BaseDeviceMemoryAllocator::reset()
{
    for (const auto& [_, heap] : m_memory_allocations)
    {
        heap->Release();
    }

    m_memory_allocations.clear();
    m_mapped_allocations.clear();
}

void Dx12BaseDeviceMemoryAllocator::map_memory_if_host_visible(const Dx12BufferResource& buffer, AllocationId id)
{
    if (buffer.get_usage() & BufferUsageBits::HostVisible)
    {
        uint8_t* mapped_data;

        // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map
        // pReadRange
        // A null pointer indicates the entire subresource might be read by the CPU.
        DX12_CHECK(buffer.handle()->Map(0, nullptr, reinterpret_cast<void**>(&mapped_data)));

        m_mapped_allocations.insert({id, mapped_data});
    }
}

//
// Dx12AliasedDeviceMemoryAllocator
//

Dx12AliasedDeviceMemoryAllocator::Dx12AliasedDeviceMemoryAllocator(bool host_visible, std::string name)
    : m_host_visible(host_visible)
    , m_name(std::move(name))
{
    m_heap_type = m_host_visible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
}

Dx12AliasedDeviceMemoryAllocator::~Dx12AliasedDeviceMemoryAllocator()
{
    free_if_allocated();
}

void Dx12AliasedDeviceMemoryAllocator::allocate_buffer_resource(const BufferResource& buffer, size_t offset)
{
    const Dx12BufferResource& native_buffer = dynamic_cast<const Dx12BufferResource&>(buffer);
    m_buffer_infos.emplace_back(
        const_cast<Dx12BufferResource&>(native_buffer), native_buffer.get_memory_requirements().size, offset);
}

void Dx12AliasedDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image, size_t offset)
{
    const Dx12ImageResource& native_image = dynamic_cast<const Dx12ImageResource&>(image);
    m_image_infos.emplace_back(
        const_cast<Dx12ImageResource&>(native_image), native_image.get_memory_requirements().size, offset);
}

uint8_t* Dx12AliasedDeviceMemoryAllocator::get_mapped_memory() const
{
    // TODO:
    return nullptr;
}

void Dx12AliasedDeviceMemoryAllocator::allocate()
{
    if (m_buffer_infos.empty() && m_image_infos.empty())
    {
        return;
    }

    uint64_t max_size = 0;

    for (const BufferInfo& info : m_buffer_infos)
    {
        max_size = std::max(info.offset + info.size, max_size);
    }

    for (const ImageInfo& info : m_image_infos)
    {
        max_size = std::max(info.offset + info.size, max_size);
    }

    if (max_size > m_size)
    {
        m_size = max_size;

        allocate_memory();
    }

    bind_resources();

    m_buffer_infos.clear();
    m_image_infos.clear();
}

size_t Dx12AliasedDeviceMemoryAllocator::get_allocated_size() const
{
    return m_size;
}

void Dx12AliasedDeviceMemoryAllocator::bind_resources()
{
    for (BufferInfo& info : m_buffer_infos)
    {
        info.resource.create_placed_resource(m_heap, info.offset);
    }

    for (ImageInfo& info : m_image_infos)
    {
        info.resource.create_placed_resource(m_heap, info.offset);
    }
}

void Dx12AliasedDeviceMemoryAllocator::allocate_memory()
{
    MIZU_PROFILE_SCOPED;

    Renderer::wait_idle();

    free_if_allocated();

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = m_heap_type;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 0;
    heap_properties.VisibleNodeMask = 0;

    D3D12_HEAP_DESC heap_desc{};
    heap_desc.SizeInBytes = m_size;
    heap_desc.Properties = heap_properties;
    heap_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    heap_desc.Flags = D3D12_HEAP_FLAG_NONE;

    Dx12Context.device->handle()->CreateHeap(&heap_desc, IID_PPV_ARGS(&m_heap));

    if (m_host_visible)
    {
        // TODO:
    }
}

void Dx12AliasedDeviceMemoryAllocator::free_if_allocated()
{
    if (m_heap)
    {
        m_heap->Release();
        m_heap = nullptr;
    }
}

} // namespace Mizu::Dx12