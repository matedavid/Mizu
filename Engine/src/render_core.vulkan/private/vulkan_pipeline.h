#pragma once

#include <vector>

#include "render_core/rhi/pipeline.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanBufferResource;

class VulkanPipeline : public Pipeline
{
  public:
    VulkanPipeline(const GraphicsPipelineDescription& desc);
    VulkanPipeline(const ComputePipelineDescription& desc);
    VulkanPipeline(const RayTracingPipelineDescription& desc);

    ~VulkanPipeline() override;

    PipelineType get_pipeline_type() const override { return m_pipeline_type; }

    static VkPipelineBindPoint get_vulkan_pipeline_bind_point(PipelineType type);

    // RayTracingPipeline Specific
    const VkStridedDeviceAddressRegionKHR& get_ray_generation_region() const;
    const VkStridedDeviceAddressRegionKHR& get_miss_region() const;
    const VkStridedDeviceAddressRegionKHR& get_hit_region() const;
    const VkStridedDeviceAddressRegionKHR& get_call_region() const;

    std::optional<DescriptorBindingInfo> get_push_constant_info() const;

    VkPipeline handle() const { return m_pipeline; }
    VkPipelineLayout get_pipeline_layout() const { return m_pipeline_layout; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    PipelineType m_pipeline_type;
    std::optional<DescriptorBindingInfo> m_push_constant_info;

    // RayTracingPipeline Specific
    std::unique_ptr<VulkanBufferResource> m_sbt_buffer;
    VkStridedDeviceAddressRegionKHR m_ray_generation_region{};
    VkStridedDeviceAddressRegionKHR m_miss_region{};
    VkStridedDeviceAddressRegionKHR m_hit_region{};
    VkStridedDeviceAddressRegionKHR m_call_region{};

    void get_push_constant_info_if_exists(const std::span<DescriptorBindingInfo>& descriptors);
};

} // namespace Mizu::Vulkan