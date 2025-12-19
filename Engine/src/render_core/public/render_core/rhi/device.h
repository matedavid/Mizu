#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "mizu_render_core_module.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class AliasedDeviceMemoryAllocator;
class BufferResource;
class Fence;
class Framebuffer;
class ImageResource;
class Pipeline;
class ResourceGroup;
class ResourceGroupBuilder;
class SamplerState;
class Semaphore;
class Shader;
class Swapchain;
struct AccelerationStructureDescription;
struct BufferDescription;
struct ComputePipelineDescription;
struct FramebufferDescription;
struct GraphicsPipelineDescription;
struct ImageDescription;
struct RayTracingPipelineDescription;
struct SamplerStateDescription;
struct ShaderDescription;
struct SwapchainDescription;

enum class GraphicsApi
{
    Dx12,
    Vulkan,
};

struct Version
{
    uint32_t major = 0;
    uint32_t minor = 1;
    uint32_t patch = 0;
};

struct VulkanSpecificConfiguration
{
    std::span<const char*> instance_extensions{};
};

struct Dx12SpecificConfiguration
{
};

using ApiSpecificConfiguration = std::variant<Dx12SpecificConfiguration, VulkanSpecificConfiguration>;

struct DeviceCreationDescription
{
    GraphicsApi api;
    ApiSpecificConfiguration specific_config;

    std::string_view application_name;
    Version application_version;

    std::string_view engine_name;
    Version engine_version;
};

struct DeviceProperties
{
    std::string name;

    bool depth_clamp_enabled;
    bool async_compute;
    bool ray_tracing_hardware;
};

class MIZU_RENDER_CORE_API Device
{
  public:
    virtual ~Device() = default;

    static Device* create(const DeviceCreationDescription& desc);

    virtual void wait_idle() const = 0;

    virtual const DeviceProperties& get_properties() const = 0;

    // Creation functions

    virtual std::shared_ptr<BufferResource> create_buffer(const BufferDescription& desc) const = 0;
    virtual std::shared_ptr<ImageResource> create_image(const ImageDescription& desc) const = 0;
    virtual std::shared_ptr<AccelerationStructure> create_acceleration_structure(
        const AccelerationStructureDescription& desc) const = 0;

    virtual std::shared_ptr<CommandBuffer> create_command_buffer(CommandBufferType type) const = 0;
    virtual std::shared_ptr<Framebuffer> create_framebuffer(const FramebufferDescription& desc) const = 0;
    virtual std::shared_ptr<Shader> create_shader(const ShaderDescription& desc) const = 0;
    virtual std::shared_ptr<SamplerState> create_sampler_state(const SamplerStateDescription& desc) const = 0;

    virtual std::shared_ptr<Pipeline> create_pipeline(const GraphicsPipelineDescription& desc) const = 0;
    virtual std::shared_ptr<Pipeline> create_pipeline(const ComputePipelineDescription& desc) const = 0;
    virtual std::shared_ptr<Pipeline> create_pipeline(const RayTracingPipelineDescription& desc) const = 0;

    virtual std::shared_ptr<ResourceGroup> create_resource_group(const ResourceGroupBuilder& builder) const = 0;

    virtual std::shared_ptr<ShaderResourceView> create_srv(
        const std::shared_ptr<ImageResource>& resource,
        ImageResourceViewRange range = {}) const = 0;
    virtual std::shared_ptr<ShaderResourceView> create_srv(const std::shared_ptr<BufferResource>& resource) const = 0;

    virtual std::shared_ptr<UnorderedAccessView> create_uav(
        const std::shared_ptr<ImageResource>& resource,
        ImageResourceViewRange range = {}) const = 0;
    virtual std::shared_ptr<UnorderedAccessView> create_uav(const std::shared_ptr<BufferResource>& resource) const = 0;

    virtual std::shared_ptr<ConstantBufferView> create_cbv(const std::shared_ptr<BufferResource>& resource) const = 0;

    virtual std::shared_ptr<RenderTargetView> create_rtv(
        const std::shared_ptr<ImageResource>& resource,
        ImageResourceViewRange range = {}) const = 0;
    virtual std::shared_ptr<RenderTargetView> create_rtv(
        const std::shared_ptr<ImageResource>& resource,
        ImageFormat format,
        ImageResourceViewRange range = {}) const = 0;

    virtual std::shared_ptr<Semaphore> create_semaphore() const = 0;
    virtual std::shared_ptr<Fence> create_fence(bool signaled = true) const = 0;

    virtual std::shared_ptr<Swapchain> create_swapchain(const SwapchainDescription& desc) const = 0;

    virtual std::shared_ptr<AliasedDeviceMemoryAllocator> create_aliased_memory_allocator(
        bool host_visible = false,
        std::string name = "") const = 0;
};

} // namespace Mizu

typedef Mizu::Device* (*create_rhi_device_func)(const Mizu::DeviceCreationDescription& desc);