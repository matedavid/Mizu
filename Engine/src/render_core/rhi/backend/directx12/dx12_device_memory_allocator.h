#pragma once

#include <unordered_map>

#include "render_core/rhi/device_memory_allocator.h"

#include "render_core/rhi/backend/directx12/dx12_buffer_resource.h"
#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/backend/directx12/dx12_image_resource.h"

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

class Dx12AliasedDeviceMemoryAllocator : public AliasedDeviceMemoryAllocator
{
  public:
    Dx12AliasedDeviceMemoryAllocator(bool host_visible, std::string name);
    ~Dx12AliasedDeviceMemoryAllocator() override;

    void allocate_buffer_resource(const BufferResource& buffer, size_t offset) override;
    void allocate_image_resource(const ImageResource& image, size_t offset) override;

    uint8_t* get_mapped_memory() const override;

    void allocate() override;

    size_t get_allocated_size() const override;

  private:
    bool m_host_visible;
    std::string m_name;

    ID3D12Heap* m_heap = nullptr;
    D3D12_HEAP_TYPE m_heap_type;
    uint64_t m_size = 0;

    // TODO: uint8_t* m_mapped_data = nullptr;

    struct MemoryInfo
    {
        D3D12_RESOURCE_DESC resource_desc;
        size_t size, offset;
    };

    struct BufferInfo : public MemoryInfo
    {
        Dx12BufferResource& resource;

        BufferInfo(Dx12BufferResource& resource_, size_t size, size_t offset) : resource(resource_)
        {
            this->size = size;
            this->offset = offset;
            this->resource_desc = resource.get_resource_description();
        }
    };

    struct ImageInfo : public MemoryInfo
    {
        Dx12ImageResource& resource;

        ImageInfo(Dx12ImageResource& resource_, size_t size, size_t offset) : resource(resource_)
        {
            this->size = size;
            this->offset = offset;
            this->resource_desc = resource.get_resource_description();
        }
    };

    std::vector<BufferInfo> m_buffer_infos;
    std::vector<ImageInfo> m_image_infos;

    void bind_resources();
    void allocate_memory();
    void free_if_allocated();
};

} // namespace Mizu::Dx12