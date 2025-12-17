#pragma once

#include <unordered_map>

#include "backend/vulkan/vulkan_core.h"
#include "render_core/rhi/device_memory_allocator.h"

namespace Mizu::Vulkan
{

class VulkanBaseDeviceMemoryAllocator : public virtual BaseDeviceMemoryAllocator
{
  public:
    VulkanBaseDeviceMemoryAllocator() = default;
    ~VulkanBaseDeviceMemoryAllocator() override;

    AllocationInfo allocate_buffer_resource(const BufferResource& buffer) override;
    AllocationInfo allocate_image_resource(const ImageResource& image) override;

    uint8_t* get_mapped_memory(AllocationId id) const override;

    void release(AllocationId id) override;
    void reset() override;

  private:
    std::unordered_map<AllocationId, VkDeviceMemory> m_memory_allocations;
    std::unordered_map<AllocationId, void*> m_mapped_allocations;
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

} // namespace Mizu::Vulkan
