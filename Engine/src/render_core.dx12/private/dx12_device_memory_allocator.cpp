#include "dx12_device_memory_allocator.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"

#include "dx12_context.h"

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
    DX12_CHECK(Dx12Context.device->handle()->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap)));

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
    DX12_CHECK(Dx12Context.device->handle()->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap)));

    AllocationInfo info{};
    info.id = AllocationId();
    info.size = memory_requirements.size;
    info.offset = 0;
    info.device_memory = (void*)heap;

    m_memory_allocations.insert({info.id, heap});

    return info;
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
}

void Dx12BaseDeviceMemoryAllocator::reset()
{
    for (const auto& [_, heap] : m_memory_allocations)
    {
        heap->Release();
    }

    m_memory_allocations.clear();
}

//
// Dx12TransientMemoryPool
//

Dx12TransientMemoryPool::Dx12TransientMemoryPool(std::string_view name) : m_name(name) {}

Dx12TransientMemoryPool::~Dx12TransientMemoryPool()
{
    reset();
}

void Dx12TransientMemoryPool::place_buffer(BufferResource& buffer, size_t offset)
{
    Dx12BufferResource& native_buffer = static_cast<Dx12BufferResource&>(buffer);
    m_buffer_infos.emplace_back(native_buffer, native_buffer.get_memory_requirements().size, offset);
}

void Dx12TransientMemoryPool::place_image(ImageResource& image, size_t offset)
{
    Dx12ImageResource& native_image = static_cast<Dx12ImageResource&>(image);
    m_image_infos.emplace_back(native_image, native_image.get_memory_requirements().size, offset);
}

void Dx12TransientMemoryPool::commit()
{
    MIZU_PROFILE_SCOPED;

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

        free_if_allocated();
        allocate_memory();
    }

    bind_resources();

    m_buffer_infos.clear();
    m_image_infos.clear();
}

void Dx12TransientMemoryPool::reset()
{
    Dx12Context.device->wait_idle();

    free_if_allocated();

    m_buffer_infos.clear();
    m_image_infos.clear();
}

size_t Dx12TransientMemoryPool::get_committed_size() const
{
    return m_size;
}

void Dx12TransientMemoryPool::bind_resources()
{
    for (const BufferInfo& info : m_buffer_infos)
    {
        if (info.resource.get_allocation_info().device_memory != nullptr)
            continue;

        info.resource.create_placed_resource(m_heap, info.offset);

        AllocationInfo allocation{};
        allocation.size = info.size;
        allocation.offset = info.offset;
        allocation.device_memory = m_heap;
        info.resource.set_allocation_info(allocation);
    }

    for (const ImageInfo& info : m_image_infos)
    {
        if (info.resource.get_allocation_info().device_memory != nullptr)
            continue;

        info.resource.create_placed_resource(m_heap, info.offset);

        AllocationInfo allocation{};
        allocation.size = info.size;
        allocation.offset = info.offset;
        allocation.device_memory = m_heap;
        info.resource.set_allocation_info(allocation);
    }
}

void Dx12TransientMemoryPool::allocate_memory()
{
    MIZU_PROFILE_SCOPED;

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 0;
    heap_properties.VisibleNodeMask = 0;

    D3D12_HEAP_DESC heap_desc{};
    heap_desc.SizeInBytes = m_size;
    heap_desc.Properties = heap_properties;
    heap_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    heap_desc.Flags = D3D12_HEAP_FLAG_NONE;

    DX12_CHECK(Dx12Context.device->handle()->CreateHeap(&heap_desc, IID_PPV_ARGS(&m_heap)));
}

void Dx12TransientMemoryPool::free_if_allocated()
{
    if (m_heap)
    {
        m_heap->Release();
        m_heap = nullptr;
    }
}

} // namespace Mizu::Dx12