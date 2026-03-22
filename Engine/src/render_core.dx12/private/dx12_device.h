#pragma once

#include <memory>
#include <mutex>
#include <stack>
#include <thread>
#include <unordered_map>
#include <vector>

#include "render_core/rhi/device.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Device : public Device
{
  public:
    Dx12Device(const DeviceCreationDescription& desc);
    ~Dx12Device() override;

    GraphicsApi get_api() const override { return GraphicsApi::Dx12; }
    const DeviceProperties& get_properties() const override { return m_properties; }

    bool is_queue_available(CommandBufferType type) const;
    ID3D12CommandQueue* get_queue(CommandBufferType type) const;

    ID3D12CommandQueue* get_graphics_queue() const { return m_graphics_queue; }
    ID3D12CommandQueue* get_compute_queue() const { return m_compute_queue; }
    ID3D12CommandQueue* get_transfer_queue() const { return m_transfer_queue; }

    ID3D12GraphicsCommandList7* allocate_command_list(CommandBufferType type, uint32_t frame_idx);
    void free_command_list(ID3D12GraphicsCommandList7* command_list, CommandBufferType type);

    ID3D12CommandAllocator* get_thread_command_allocator(CommandBufferType type, uint32_t frame_idx);

    ID3D12Device* handle() const { return m_device; }

    // Operations

    void prepare_frame(uint32_t frame_idx) override;
    void wait_idle() const override;

    // Creation functions

    std::shared_ptr<BufferResource> create_buffer(const BufferDescription& desc) const override;
    std::shared_ptr<ImageResource> create_image(const ImageDescription& desc) const override;
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

    std::shared_ptr<ResourceGroup> create_resource_group(const ResourceGroupBuilder& builder) const override;
    std::shared_ptr<DescriptorSet> allocate_descriptor_set(
        DescriptorSetLayoutHandle layout,
        DescriptorSetAllocationType type,
        uint32_t variable_count = 0) const override;

    std::shared_ptr<Semaphore> create_semaphore() const override;
    std::shared_ptr<Fence> create_fence(bool signaled) const override;

    std::shared_ptr<Swapchain> create_swapchain(const SwapchainDescription& desc) const override;

    std::shared_ptr<AliasedDeviceMemoryAllocator> create_aliased_memory_allocator(
        bool host_visible = false,
        std::string name = "") const override;

    std::shared_ptr<TransientMemoryPool> create_transient_memory_pool(std::string_view name = "") const override;

    // Other

    MemoryRequirements get_buffer_memory_requirements(const BufferDescription& desc) const override;
    MemoryRequirements get_image_memory_requirements(const ImageDescription& desc) const override;

  private:
    IDXCoreAdapterFactory* m_factory = nullptr;
    ID3D12Device* m_device = nullptr;
    IDXGIAdapter1* m_adapter = nullptr;

    DeviceProperties m_properties{};

    ID3D12CommandQueue* m_graphics_queue = nullptr;
    ID3D12CommandQueue* m_compute_queue = nullptr;
    ID3D12CommandQueue* m_transfer_queue = nullptr;

    struct ThreadCommandInfo
    {
        struct Type
        {
            std::vector<ID3D12CommandAllocator*> command_allocators{};
            std::stack<ID3D12GraphicsCommandList7*> available_command_buffers{};
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

    void create_queues();
    void create_per_thread_command_info();
    ThreadCommandInfo create_thread_command_info();
    ThreadCommandInfo::Type& get_thread_command_info(std::thread::id id, CommandBufferType type);
    void retrieve_device_capabilities();
};

} // namespace Mizu::Dx12
