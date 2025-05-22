#pragma once

#include "render_core/rhi/rtx/ray_tracing_pipeline.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.h"

namespace Mizu::Vulkan
{

class VulkanRayTracingPipeline : public RayTracingPipeline
{
  public:
    VulkanRayTracingPipeline(Description desc);
    ~VulkanRayTracingPipeline() override;

    VkPipeline handle() const { return m_handle; }

  private:
    VkPipeline m_handle{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    Description m_description;

    void create_pipeline_layout();
};

} // namespace Mizu::Vulkan