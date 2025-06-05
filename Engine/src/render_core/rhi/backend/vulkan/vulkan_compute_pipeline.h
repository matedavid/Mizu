#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/compute_pipeline.h"

namespace Mizu::Vulkan
{

// Forward declaration
class VulkanShader;

class VulkanComputePipeline : public ComputePipeline
{
  public:
    VulkanComputePipeline(const Description& desc);
    ~VulkanComputePipeline() override;

    VkPipelineLayout get_pipeline_layout() const { return m_pipeline_layout; }
    VkPipeline handle() const { return m_pipeline; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::vector<VkDescriptorSetLayout> m_set_layouts{};

    std::shared_ptr<VulkanShader> m_shader;

    void create_pipeline_layout();
};

} // namespace Mizu::Vulkan