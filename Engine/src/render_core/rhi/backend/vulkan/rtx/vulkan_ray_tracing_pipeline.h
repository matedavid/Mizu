#pragma once

#include "render_core/rhi/rtx/ray_tracing_pipeline.h"

#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/vulkan/vulkan_pipeline.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanBufferResource;
class VulkanShader;

class VulkanRayTracingPipeline : public RayTracingPipeline, public IVulkanPipeline
{
  public:
    VulkanRayTracingPipeline(Description desc);
    ~VulkanRayTracingPipeline() override;

    VkStridedDeviceAddressRegionKHR get_ray_generation_region() const { return m_ray_generation_region; }
    VkStridedDeviceAddressRegionKHR get_miss_region() const { return m_miss_region; }
    VkStridedDeviceAddressRegionKHR get_hit_region() const { return m_hit_region; }
    VkStridedDeviceAddressRegionKHR get_call_region() const { return m_call_region; }

    VkPipeline handle() const override { return m_pipeline; }
    VkPipelineLayout get_pipeline_layout() const override { return m_pipeline_layout; }
    VkDescriptorSetLayout get_descriptor_set_layout(uint32_t set) const override
    {
        MIZU_ASSERT(set < m_set_layouts.size(), "Invalid set ({} >= {})", set, m_set_layouts.size());
        return m_set_layouts[set];
    }
    const ShaderGroup& get_shader_group() const override { return m_shader_group; }
    VkPipelineBindPoint get_pipeline_bind_point() const override { return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::vector<VkDescriptorSetLayout> m_set_layouts{};

    std::shared_ptr<VulkanShader> m_raygen_shader{nullptr};
    std::vector<std::shared_ptr<VulkanShader>> m_miss_shaders;
    std::vector<std::shared_ptr<VulkanShader>> m_closest_hit_shaders;

    ShaderGroup m_shader_group;
    std::unique_ptr<VulkanBufferResource> m_sbt_buffer;

    VkStridedDeviceAddressRegionKHR m_ray_generation_region{};
    VkStridedDeviceAddressRegionKHR m_miss_region{};
    VkStridedDeviceAddressRegionKHR m_hit_region{};
    VkStridedDeviceAddressRegionKHR m_call_region{};

    Description m_description;

    void create_pipeline_layout();
};

} // namespace Mizu::Vulkan