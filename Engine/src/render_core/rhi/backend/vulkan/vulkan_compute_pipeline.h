#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/compute_pipeline.h"
#include "render_core/shader/shader_group.h"

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

    const ShaderGroup& get_shader_group() const { return m_shader_group; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::vector<VkDescriptorSetLayout> m_set_layouts{};

    std::shared_ptr<VulkanShader> m_shader;
    ShaderGroup m_shader_group;

    void create_pipeline_layout();
};

} // namespace Mizu::Vulkan