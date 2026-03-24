#pragma once

#include <string_view>
#include <unordered_map>

#include "render_core/rhi/device_memory_allocator.h"

#include "dx12_buffer_resource.h"
#include "dx12_core.h"
#include "dx12_image_resource.h"

namespace Mizu::Dx12
{

class Dx12BaseDeviceMemoryAllocator
{
  public:
    Dx12BaseDeviceMemoryAllocator() = default;
    ~Dx12BaseDeviceMemoryAllocator();

    AllocationInfo allocate_buffer_resource(const BufferResource& buffer);
    AllocationInfo allocate_image_resource(const ImageResource& image);

    void release(AllocationId id);
    void reset();

  private:
    std::unordered_map<AllocationId, ID3D12Heap*> m_memory_allocations;
};

class Dx12TransientMemoryPool : public TransientMemoryPool
{
  public:
    Dx12TransientMemoryPool(std::string_view name = "");
    ~Dx12TransientMemoryPool() override;

    void place_buffer(BufferResource& buffer, size_t offset) override;
    void place_image(ImageResource& image, size_t offset) override;

    void commit() override;
    void reset() override;

    size_t get_committed_size() const override;

  private:
    std::string m_name;

    ID3D12Heap* m_heap = nullptr;
    uint64_t m_size = 0;

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