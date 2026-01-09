#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "mizu_render_core_module.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/descriptors.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class AliasedDeviceMemoryAllocator;
class Fence;
class Framebuffer;
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

    virtual GraphicsApi get_api() const = 0;
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
    virtual std::shared_ptr<DescriptorSet> allocate_descriptor_set(
        std::span<DescriptorItem> layout,
        DescriptorSetAllocationType type) const = 0;
    virtual void reset_transient_descriptors() = 0;

    virtual std::shared_ptr<Semaphore> create_semaphore() const = 0;
    virtual std::shared_ptr<Fence> create_fence(bool signaled = true) const = 0;

    virtual std::shared_ptr<Swapchain> create_swapchain(const SwapchainDescription& desc) const = 0;

    virtual std::shared_ptr<AliasedDeviceMemoryAllocator> create_aliased_memory_allocator(
        bool host_visible = false,
        std::string name = "") const = 0;
};

} // namespace Mizu

typedef Mizu::Device* (*create_rhi_device_func)(const Mizu::DeviceCreationDescription& desc);