#pragma once

#include <mutex>
#include <optional>
#include <stack>

#include "render_core/rhi/device.h"

#include "vulkan_core.h"
#include "vulkan_queue.h"

namespace Mizu::Vulkan
{

class VulkanDevice : public Device
{
  public:
    VulkanDevice(const DeviceCreationDescription& desc);
    ~VulkanDevice() override;

    void wait_idle() const override;

    GraphicsApi get_api() const override { return GraphicsApi::Vulkan; }
    const DeviceProperties& get_properties() const override { return m_properties; }

    VkCommandBuffer allocate_command_buffer(CommandBufferType type);
    std::vector<VkCommandBuffer> allocate_command_buffers(uint32_t count, CommandBufferType type);

    void free_command_buffer(VkCommandBuffer command_buffer, CommandBufferType type);
    void free_command_buffers(std::span<VkCommandBuffer> command_buffers, CommandBufferType type);

    std::optional<uint32_t> find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties) const;

    std::shared_ptr<VulkanQueue> get_graphics_queue() const;
    std::shared_ptr<VulkanQueue> get_compute_queue() const;
    std::shared_ptr<VulkanQueue> get_transfer_queue() const;

    VkDevice handle() const { return m_device; }
    VkPhysicalDevice get_physical_device() const { return m_physical_device; }
    VkInstance get_instance() const { return m_instance; }

    // Creation functions

    std::shared_ptr<BufferResource> create_buffer(const BufferDescription& desc) const override;
    std::shared_ptr<ImageResource> create_image(const ImageDescription& desc) const override;
    std::shared_ptr<AccelerationStructure> create_acceleration_structure(
        const AccelerationStructureDescription& desc) const override;

    std::shared_ptr<CommandBuffer> create_command_buffer(CommandBufferType type) const override;
    std::shared_ptr<Framebuffer> create_framebuffer(const FramebufferDescription& desc) const override;
    std::shared_ptr<Shader> create_shader(const ShaderDescription& desc) const override;
    std::shared_ptr<SamplerState> create_sampler_state(const SamplerStateDescription& desc) const override;

    std::shared_ptr<Pipeline> create_pipeline(const GraphicsPipelineDescription& desc) const override;
    std::shared_ptr<Pipeline> create_pipeline(const ComputePipelineDescription& desc) const override;
    std::shared_ptr<Pipeline> create_pipeline(const RayTracingPipelineDescription& desc) const override;

    std::shared_ptr<ResourceGroup> create_resource_group(const ResourceGroupBuilder& builder) const override;

    std::shared_ptr<ShaderResourceView> create_srv(
        const std::shared_ptr<ImageResource>& resource,
        ImageResourceViewRange range) const override;
    std::shared_ptr<ShaderResourceView> create_srv(const std::shared_ptr<BufferResource>& resource) const override;

    std::shared_ptr<UnorderedAccessView> create_uav(
        const std::shared_ptr<ImageResource>& resource,
        ImageResourceViewRange range) const override;
    std::shared_ptr<UnorderedAccessView> create_uav(const std::shared_ptr<BufferResource>& resource) const override;

    std::shared_ptr<ConstantBufferView> create_cbv(const std::shared_ptr<BufferResource>& resource) const override;

    std::shared_ptr<RenderTargetView> create_rtv(
        const std::shared_ptr<ImageResource>& resource,
        ImageResourceViewRange range) const override;
    std::shared_ptr<RenderTargetView> create_rtv(
        const std::shared_ptr<ImageResource>& resource,
        ImageFormat format,
        ImageResourceViewRange range) const override;

    std::shared_ptr<Semaphore> create_semaphore() const override;
    std::shared_ptr<Fence> create_fence(bool signaled) const override;

    std::shared_ptr<Swapchain> create_swapchain(const SwapchainDescription& desc) const override;

    std::shared_ptr<AliasedDeviceMemoryAllocator> create_aliased_memory_allocator(
        bool host_visible = false,
        std::string name = "") const override;

  private:
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};

    mutable std::mutex m_device_mutex;
    DeviceProperties m_properties{};

    struct QueueFamilies
    {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    };
    QueueFamilies m_queue_families{};

    std::shared_ptr<VulkanQueue> m_graphics_queue = nullptr;
    std::shared_ptr<VulkanQueue> m_compute_queue = nullptr;
    std::shared_ptr<VulkanQueue> m_transfer_queue = nullptr;

    struct ThreadCommandInfo
    {
        struct Type
        {
            VkCommandPool command_pool = VK_NULL_HANDLE;
            std::stack<VkCommandBuffer> available_command_buffers{};
            uint32_t command_buffers_in_usage = 0;
        };

        Type graphics;
        Type compute;
        Type transfer;

        Type& get_type(CommandBufferType type)
        {
            switch (type)
            {
            case CommandBufferType::Graphics:
                return graphics;
            case CommandBufferType::Compute:
                return compute;
            case CommandBufferType::Transfer:
                return transfer;
            }

            MIZU_UNREACHABLE("Invalid CommandBufferType");
        }
    };

    std::vector<ThreadCommandInfo> m_per_thread_command_info;

    std::stack<uint32_t> m_available_per_thread_command_info_idx;
    std::unordered_map<std::thread::id, uint32_t> m_thread_to_command_info_map;
    std::mutex m_assign_thread_info_mutex;

    void create_instance(const DeviceCreationDescription& desc, std::span<const char*> extensions);
    void select_physical_device();
    void create_device(std::span<const char*> instance_extensions);

    ThreadCommandInfo create_thread_command_info();
    ThreadCommandInfo::Type& get_thread_command_info(std::thread::id id, CommandBufferType type);
};

} // namespace Mizu::Vulkan
