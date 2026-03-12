#pragma once

#include <string_view>
#include <unordered_map>

#include "render_core/rhi/device_memory_allocator.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

class VulkanBufferResource;
class VulkanImageResource;

class VulkanBaseDeviceMemoryAllocator
{
  public:
    VulkanBaseDeviceMemoryAllocator() = default;
    ~VulkanBaseDeviceMemoryAllocator();

    AllocationInfo allocate_buffer_resource(const BufferResource& buffer);
    AllocationInfo allocate_image_resource(const ImageResource& image);

    void release(AllocationId id);
    void reset();

  private:
    std::unordered_map<AllocationId, VkDeviceMemory> m_memory_allocations;
};

class VulkanAliasedDeviceMemoryAllocator : public AliasedDeviceMemoryAllocator
{
  public:
    VulkanAliasedDeviceMemoryAllocator(bool host_visible, std::string name = "");
    ~VulkanAliasedDeviceMemoryAllocator() override;

    void allocate_buffer_resource(const BufferResource& buffer, size_t offset) override;
    void allocate_image_resource(const ImageResource& image, size_t offset) override;

    uint8_t* get_mapped_memory() const override;

    void allocate() override;

    size_t get_allocated_size() const override;

  private:
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    size_t m_size = 0;
    std::string m_name;

    VkMemoryPropertyFlags m_memory_property_flags = 0;
    uint32_t m_memory_type_index = 0;

    void* m_mapped_data = nullptr;

    struct MemoryInfo
    {
        size_t size;
        size_t offset;
        uint32_t memory_type_bits;
    };

    struct BufferInfo : MemoryInfo
    {
        VkBuffer buffer;
        VkBufferUsageFlags usage;
    };

    struct ImageInfo : MemoryInfo
    {
        VkImage image;
        VkImageUsageFlags usage;
    };

    std::vector<BufferInfo> m_buffer_infos;
    std::vector<ImageInfo> m_image_infos;

    void bind_resources();

    void allocate_memory(size_t size, VkMemoryAllocateFlags allocate_flags, uint32_t memory_type_index);
    void free_if_allocated();
};

class VulkanTransientMemoryPool : public TransientMemoryPool
{
  public:
    VulkanTransientMemoryPool(std::string_view name = "");
    ~VulkanTransientMemoryPool() override;

    void place_buffer(BufferResource& buffer, size_t offset) override;
    void place_image(ImageResource& image, size_t offset) override;

    void commit() override;
    void reset() override;

    size_t get_committed_size() const override;

  private:
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    size_t m_size = 0;
    std::string m_name;

    uint32_t m_memory_type_index = 0;

    struct MemoryInfo
    {
        size_t size;
        size_t offset;
        uint32_t memory_type_bits;
    };

    struct BufferInfo : public MemoryInfo
    {
        VulkanBufferResource& buffer;

        BufferInfo(VulkanBufferResource& resource_, size_t size, size_t offset, uint32_t memory_type_bits)
            : buffer(resource_)
        {
            this->size = size;
            this->offset = offset;
            this->memory_type_bits = memory_type_bits;
        }
    };

    struct ImageInfo : public MemoryInfo
    {
        VulkanImageResource& image;

        ImageInfo(VulkanImageResource& resource_, size_t size, size_t offset, uint32_t memory_type_bits)
            : image(resource_)
        {
            this->size = size;
            this->offset = offset;
            this->memory_type_bits = memory_type_bits;
        }
    };

    std::vector<BufferInfo> m_buffer_infos;
    std::vector<ImageInfo> m_image_infos;

    void bind_resources();
    void allocate_memory(size_t size, VkMemoryAllocateFlags allocate_flags, uint32_t memory_type_index);
    void free_if_allocated();
};

} // namespace Mizu::Vulkan
