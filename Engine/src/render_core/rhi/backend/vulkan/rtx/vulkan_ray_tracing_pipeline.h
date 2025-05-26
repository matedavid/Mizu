#pragma once

#include "render_core/rhi/rtx/ray_tracing_pipeline.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanBufferResource;

class VulkanRayTracingPipeline : public RayTracingPipeline
{
  public:
    VulkanRayTracingPipeline(Description desc);
    ~VulkanRayTracingPipeline() override;

    VkPipeline handle() const { return m_handle; }

  private:
    VkPipeline m_handle{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::unique_ptr<VulkanBufferResource> m_sbt_buffer;

    VkStridedDeviceAddressRegionKHR m_ray_generation_region{};
    VkStridedDeviceAddressRegionKHR m_miss_region{};
    VkStridedDeviceAddressRegionKHR m_hit_region{};
    VkStridedDeviceAddressRegionKHR m_call_region{};

    Description m_description;

    void create_pipeline_layout();
};

} // namespace Mizu::Vulkan