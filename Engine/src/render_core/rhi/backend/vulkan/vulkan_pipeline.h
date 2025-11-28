#pragma once

#include <vector>

#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/shader/shader_group.h"

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

    VkPipeline handle() const { return m_pipeline; }
    VkPipelineLayout get_pipeline_layout() const { return m_pipeline_layout; }

    VkDescriptorSetLayout get_descriptor_set_layout(uint32_t set) const
    {
        MIZU_ASSERT(set < m_set_layouts.size(), "Invalid set ({} >= {})", set, m_set_layouts.size());
        return m_set_layouts[set];
    }

    const ShaderGroup& get_shader_group() const { return m_shader_group; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    ShaderGroup m_shader_group;
    std::vector<VkDescriptorSetLayout> m_set_layouts{};
    PipelineType m_pipeline_type;

    // RayTracingPipeline Specific
    std::unique_ptr<VulkanBufferResource> m_sbt_buffer;
    VkStridedDeviceAddressRegionKHR m_ray_generation_region{};
    VkStridedDeviceAddressRegionKHR m_miss_region{};
    VkStridedDeviceAddressRegionKHR m_hit_region{};
    VkStridedDeviceAddressRegionKHR m_call_region{};
};

} // namespace Mizu::Vulkan