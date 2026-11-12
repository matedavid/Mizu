#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "mizu_render_core_module.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/descriptors.h"
#include "render_core/rhi/gpu_heap.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/memory.h"
#include "render_core/rhi/queue.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class Fence;
class GpuAllocator;
class Pipeline;
class SamplerState;
class Shader;
class Swapchain;
struct AccelerationStructureDescription;
struct BufferDescription;
struct ComputePipelineDescription;
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

struct VulkanBindingOffsets
{
    uint32_t srv_offset = 0;
    uint32_t uav_offset = 16;
    uint32_t cbv_offset = 32;
    uint32_t sampler_offset = 64;
};

struct VulkanSpecificConfiguration
{
    VulkanBindingOffsets binding_offsets;
    std::span<const char*> instance_extensions{};
};

struct Dx12SpecificConfiguration
{
};

using ApiSpecificConfiguration = std::variant<Dx12SpecificConfiguration, VulkanSpecificConfiguration>;

struct DeviceCreationDescription
{
    GraphicsApi api = GraphicsApi::Vulkan;
    ApiSpecificConfiguration specific_config = VulkanSpecificConfiguration{};
    uint32_t frames_in_flight = 1;
    bool validations_enabled = true;

    std::string_view application_name{};
    Version application_version{};

    std::string_view engine_name{};
    Version engine_version{};
};

struct DeviceAlignments
{
    uint64_t min_constant_buffer_offset = 1;
    uint64_t min_raw_buffer_offset = 1;
    uint64_t min_texture_row_pitch = 1;
    uint64_t min_texture_data_placement = 1;
};

struct RayTracingProperties
{
    bool supported = false;
    uint64_t acceleration_structure_instance_size = 0;
};

struct SparseResourcesProperties
{
    bool buffers = false;
    bool images = false;
    bool non_resident_strict = false;
    uint64_t address_space_size = 0;
};

struct DeviceProperties
{
    std::string name;

    bool depth_clamp = false;
    bool async_compute = false;
    bool async_transfer = false;

    uint32_t max_bindless_descriptors = 0;

    DeviceAlignments alignments{};
    RayTracingProperties ray_tracing{};
    SparseResourcesProperties sparse{};
};

class MIZU_RENDER_CORE_API Device
{
  public:
    Device();
    virtual ~Device();

    static Device* create(const DeviceCreationDescription& desc);
    static void free();

    virtual GraphicsApi get_api() const = 0;
    virtual const DeviceProperties& get_properties() const = 0;

    // Operations

    virtual void prepare_frame(uint32_t frame_in_flight_idx) = 0;
    virtual void wait_idle() const = 0;

    virtual Queue& get_queue(CommandBufferType type) const = 0;

    // Creation functions

    virtual std::shared_ptr<BufferResource> create_buffer(
        const BufferDescription& desc,
        const MemoryPlacement& placement) const = 0;
    virtual std::shared_ptr<ImageResource> create_image(const ImageDescription& desc, const MemoryPlacement& placement)
        const = 0;

    virtual std::shared_ptr<BufferResource> create_sparse_buffer(const BufferDescription& desc) const = 0;
    virtual std::shared_ptr<ImageResource> create_sparse_image(const ImageDescription& desc) const = 0;

    std::shared_ptr<BufferResource> create_buffer(
        const BufferDescription& desc,
        HeapDomain domain = HeapDomain::DeviceLocal) const;
    std::shared_ptr<ImageResource> create_image(
        const ImageDescription& desc,
        HeapDomain domain = HeapDomain::DeviceLocal) const;

    virtual std::shared_ptr<AccelerationStructure> create_acceleration_structure(
        const AccelerationStructureDescription& desc) const = 0;

    virtual std::shared_ptr<CommandBuffer> create_command_buffer(CommandBufferType type) const = 0;
    virtual std::shared_ptr<Shader> create_shader(const ShaderDescription& desc) const = 0;
    virtual std::shared_ptr<SamplerState> create_sampler_state(const SamplerStateDescription& desc) const = 0;

    virtual std::shared_ptr<Pipeline> create_pipeline(const GraphicsPipelineDescription& desc) const = 0;
    virtual std::shared_ptr<Pipeline> create_pipeline(const ComputePipelineDescription& desc) const = 0;
    virtual std::shared_ptr<Pipeline> create_pipeline(const RayTracingPipelineDescription& desc) const = 0;

    virtual DescriptorSetLayoutHandle create_descriptor_set_layout(
        const DescriptorSetLayoutDescription& desc) const = 0;
    virtual PipelineLayoutHandle create_pipeline_layout(const PipelineLayoutDescription& desc) const = 0;

    std::shared_ptr<DescriptorSet> allocate_descriptor_set(
        std::span<const DescriptorItem> layout,
        DescriptorSetAllocationType type,
        uint32_t variable_count = 0) const;
    virtual std::shared_ptr<DescriptorSet> allocate_descriptor_set(
        DescriptorSetLayoutHandle layout,
        DescriptorSetAllocationType type,
        uint32_t variable_count = 0) const = 0;

    virtual std::shared_ptr<Fence> create_fence(uint64_t initial_value = 0) const = 0;

    virtual std::shared_ptr<Swapchain> create_swapchain(const SwapchainDescription& desc) const = 0;

    virtual std::shared_ptr<GpuHeap> create_gpu_heap(const GpuHeapDescription& desc) const = 0;

    // Memory

    virtual MemoryRequirements get_buffer_memory_requirements(const BufferDescription& desc) const = 0;
    virtual MemoryRequirements get_image_memory_requirements(const ImageDescription& desc) const = 0;

    virtual std::optional<MemoryTypeId> find_memory_type(const MemoryRequirements& reqs, HeapDomain domain) const = 0;

    GpuAllocator& get_default_allocator() const;

  protected:
    void destroy_default_allocator();

    std::unique_ptr<GpuAllocator> m_default_allocator;
};

} // namespace Mizu

typedef Mizu::Device* (*create_rhi_device_func)(const Mizu::DeviceCreationDescription&);
