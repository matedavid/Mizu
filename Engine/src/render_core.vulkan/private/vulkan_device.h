#pragma once

#include <mutex>
#include <optional>
#include <stack>
#include <thread>

#include "render_core/rhi/device.h"

#include "vulkan_core.h"
#include "vulkan_queue.h"

namespace Mizu::Vulkan
{

struct VulkanPhysicalDeviceInfo;

class VulkanDevice : public Device
{
  public:
    VulkanDevice(const DeviceCreationDescription& desc);
    ~VulkanDevice() override;

    GraphicsApi get_api() const override { return GraphicsApi::Vulkan; }
    const DeviceProperties& get_properties() const override { return m_properties; }

    VkCommandBuffer allocate_command_buffer(CommandBufferType type);
    std::vector<VkCommandBuffer> allocate_command_buffers(uint32_t count, CommandBufferType type);

    void free_command_buffer(VkCommandBuffer command_buffer, CommandBufferType type);
    void free_command_buffers(std::span<VkCommandBuffer> command_buffers, CommandBufferType type);

    std::optional<uint32_t> find_memory_type_index(HeapDomain domain, uint32_t memory_type_bits) const;
    const VkPhysicalDeviceMemoryProperties& get_memory_properties() const { return m_memory_properties; }

    bool is_buffer_device_address_enabled() const { return m_optional_features.ray_tracing; }

    bool is_queue_available(CommandBufferType type) const;
    VulkanQueue& get_native_queue(CommandBufferType type) const;

    VkDevice handle() const { return m_device; }
    VkPhysicalDevice get_physical_device() const { return m_physical_device; }
    VkInstance get_instance() const { return m_instance; }

    // Operations

    void prepare_frame(uint32_t frame_in_flight_idx) override;
    void wait_idle() const override;

    Queue& get_queue(CommandBufferType type) const override;

    // Creation functions

    using Device::create_buffer;
    using Device::create_image;

    std::shared_ptr<BufferResource> create_buffer(const BufferDescription& desc, const MemoryPlacement& placement)
        const override;
    std::shared_ptr<ImageResource> create_image(const ImageDescription& desc, const MemoryPlacement& placement)
        const override;

    std::shared_ptr<BufferResource> create_sparse_buffer(const BufferDescription& desc) const override;
    std::shared_ptr<ImageResource> create_sparse_image(const ImageDescription& desc) const override;

    std::shared_ptr<AccelerationStructure> create_acceleration_structure(
        const AccelerationStructureDescription& desc) const override;

    std::shared_ptr<CommandBuffer> create_command_buffer(CommandBufferType type) const override;
    std::shared_ptr<Shader> create_shader(const ShaderDescription& desc) const override;
    std::shared_ptr<SamplerState> create_sampler_state(const SamplerStateDescription& desc) const override;

    std::shared_ptr<Pipeline> create_pipeline(const GraphicsPipelineDescription& desc) const override;
    std::shared_ptr<Pipeline> create_pipeline(const ComputePipelineDescription& desc) const override;
    std::shared_ptr<Pipeline> create_pipeline(const RayTracingPipelineDescription& desc) const override;

    DescriptorSetLayoutHandle create_descriptor_set_layout(const DescriptorSetLayoutDescription& desc) const override;
    PipelineLayoutHandle create_pipeline_layout(const PipelineLayoutDescription& desc) const override;

    std::shared_ptr<DescriptorSet> allocate_descriptor_set(
        DescriptorSetLayoutHandle layout,
        DescriptorSetAllocationType type,
        uint32_t variable_count = 0) const override;

    std::shared_ptr<Fence> create_fence(uint64_t initial_value) const override;

    std::shared_ptr<Swapchain> create_swapchain(const SwapchainDescription& desc) const override;

    std::shared_ptr<GpuHeap> create_gpu_heap(const GpuHeapDescription& desc) const override;

    // Memory

    MemoryRequirements get_buffer_memory_requirements(const BufferDescription& desc) const override;
    MemoryRequirements get_image_memory_requirements(const ImageDescription& desc) const override;

    std::optional<MemoryTypeId> find_memory_type(const MemoryRequirements& reqs, HeapDomain domain) const override;

  private:
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};

    DeviceProperties m_properties{};

    VkPhysicalDeviceMemoryProperties m_memory_properties{};

    struct OptionalFeatures
    {
        bool ray_tracing = false;
        bool sparse_buffers = false;
        bool sparse_images = false;
    };
    OptionalFeatures m_optional_features{};

    struct DeviceRequirements
    {
        bool swapchain = false;
    };

    struct QueueFamilies
    {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    };
    QueueFamilies m_queue_families{};

    std::vector<std::unique_ptr<VulkanQueue>> m_queues;
    VulkanQueue* m_graphics_queue = nullptr;
    VulkanQueue* m_compute_queue = nullptr;
    VulkanQueue* m_transfer_queue = nullptr;

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
    static const char* find_missing_requirement(VulkanPhysicalDeviceInfo& info, const DeviceRequirements& requirements);
    static bool select_queue_families(std::span<const VkQueueFamilyProperties> families, QueueFamilies& selection);
    void select_physical_device(const DeviceRequirements& requirements);
    void create_device(const DeviceRequirements& requirements);

    ThreadCommandInfo create_thread_command_info();
    ThreadCommandInfo::Type& get_thread_command_info(std::thread::id id, CommandBufferType type);
};

} // namespace Mizu::Vulkan
