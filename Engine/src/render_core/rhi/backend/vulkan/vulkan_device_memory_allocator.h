#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "render_core/rhi/device_memory_allocator.h"

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

namespace Mizu::Vulkan
{

struct VulkanAllocationInfo
{
    VkDeviceMemory memory = VK_NULL_HANDLE;
    size_t offset = 0;
};

class IVulkanDeviceMemoryAllocator : public IDeviceMemoryAllocator
{
  public:
    virtual VulkanAllocationInfo get_allocation_info(Allocation id) const = 0;
};

class VulkanBaseDeviceMemoryAllocator : public virtual BaseDeviceMemoryAllocator,
                                        public virtual IVulkanDeviceMemoryAllocator
{
  public:
    VulkanBaseDeviceMemoryAllocator() = default;
    ~VulkanBaseDeviceMemoryAllocator() override;

    Allocation allocate_buffer_resource(const BufferResource& buffer) override;
    Allocation allocate_image_resource(const ImageResource& image) override;

    void release(Allocation id) override;

    VulkanAllocationInfo get_allocation_info(Allocation id) const override;

  private:
    std::unordered_map<Allocation, VkDeviceMemory> m_memory_allocations;
};

//
//
//

class VulkanTransientImageResource : public TransientImageResource
{
  public:
    VulkanTransientImageResource(const ImageDescription& desc);

    [[nodiscard]] uint64_t get_size() const override { return m_memory_reqs.size; }
    [[nodiscard]] uint64_t get_alignment() const override { return m_memory_reqs.alignment; }

    [[nodiscard]] std::shared_ptr<ImageResource> get_resource() const override { return m_resource; }
    VkMemoryRequirements get_memory_requirements() const { return m_memory_reqs; }

  private:
    std::shared_ptr<VulkanImageResource> m_resource;
    VkMemoryRequirements m_memory_reqs{};
};

class VulkanTransientBufferResource : public TransientBufferResource
{
  public:
    VulkanTransientBufferResource(const BufferDescription& desc);

    [[nodiscard]] uint64_t get_size() const override { return m_memory_reqs.size; }
    [[nodiscard]] uint64_t get_alignment() const override { return m_memory_reqs.alignment; }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const override { return m_resource; }
    VkMemoryRequirements get_memory_requirements() const { return m_memory_reqs; }

  private:
    std::shared_ptr<VulkanBufferResource> m_resource;
    VkMemoryRequirements m_memory_reqs{};
};

class VulkanRenderGraphDeviceMemoryAllocator : public RenderGraphDeviceMemoryAllocator
{
  public:
    VulkanRenderGraphDeviceMemoryAllocator() = default;
    ~VulkanRenderGraphDeviceMemoryAllocator() override;

    void allocate_image_resource(const TransientImageResource& resource, size_t offset) override;
    void allocate_buffer_resource(const TransientBufferResource& resource, size_t offset) override;

    void allocate() override;

    size_t get_size() const override { return m_size; }

  private:
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    size_t m_size = 0;

    struct ImageAllocationInfo
    {
        std::shared_ptr<VulkanImageResource> image;
        uint32_t memory_type_bits;
        VkDeviceSize size;
        size_t offset;
    };
    std::vector<ImageAllocationInfo> m_image_allocations;

    struct BufferAllocationInfo
    {
        std::shared_ptr<VulkanBufferResource> buffer;
        uint32_t memory_type_bits;
        VkDeviceSize size;
        // TODO: not really sure I like this, having two values for size, requested_size is only used on the Staging
        // buffer when copying data into the real buffer, as sometimes the requested size does not match que size
        // requirements, causing an assert on copy_to_buffer
        VkDeviceSize requested_size;
        size_t offset;
    };
    std::vector<BufferAllocationInfo> m_buffer_allocations;

    void bind_resources();
    void free_if_allocated();
};

} // namespace Mizu::Vulkan
