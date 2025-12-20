#pragma once

#include "render_core/rhi/device.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Device : public Device
{
  public:
    Dx12Device(const DeviceCreationDescription& desc);
    ~Dx12Device() override;

    void wait_idle() const override;

    GraphicsApi get_api() const override { return GraphicsApi::Dx12; }
    const DeviceProperties& get_properties() const override { return m_properties; }

    ID3D12CommandQueue* get_graphics_queue() const { return m_graphics_queue; }
    ID3D12CommandQueue* get_compute_queue() const { return m_compute_queue; }
    ID3D12CommandQueue* get_transfer_queue() const { return m_transfer_queue; }

    ID3D12GraphicsCommandList7* allocate_command_list(CommandBufferType type);
    void free_command_list(ID3D12GraphicsCommandList7* command_list, CommandBufferType type);

    ID3D12CommandAllocator* get_command_allocator(CommandBufferType type) const;

    ID3D12Device* handle() const { return m_device; }

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
    IDXCoreAdapterFactory* m_factory = nullptr;
    ID3D12Device* m_device = nullptr;
    IDXGIAdapter1* m_adapter = nullptr;

    ID3D12CommandQueue* m_graphics_queue = nullptr;
    ID3D12CommandQueue* m_compute_queue = nullptr;
    ID3D12CommandQueue* m_transfer_queue = nullptr;

    ID3D12CommandAllocator* m_graphics_command_allocator = nullptr;
    ID3D12CommandAllocator* m_compute_command_allocator = nullptr;
    ID3D12CommandAllocator* m_transfer_command_allocator = nullptr;

    DeviceProperties m_properties{};

    void create_queues();
    void create_command_allocators();
    void retrieve_device_capabilities();
};

} // namespace Mizu::Dx12