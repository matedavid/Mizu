#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/compute_pipeline.h"

#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/vulkan/vulkan_pipeline.h"

namespace Mizu::Vulkan
{

// Forward declaration
class VulkanShader;

class VulkanComputePipeline : public ComputePipeline, public IVulkanPipeline
{
  public:
    VulkanComputePipeline(const Description& desc);
    ~VulkanComputePipeline() override;

    VkPipeline handle() const override { return m_pipeline; }
    VkPipelineLayout get_pipeline_layout() const override { return m_pipeline_layout; }
    VkDescriptorSetLayout get_descriptor_set_layout(uint32_t set) const override
    {
        MIZU_ASSERT(set < m_set_layouts.size(), "Invalid set ({} >= {})", set, m_set_layouts.size());
        return m_set_layouts[set];
    }
    const ShaderGroup& get_shader_group() const override { return m_shader_group; }
    VkPipelineBindPoint get_pipeline_bind_point() const override { return VK_PIPELINE_BIND_POINT_COMPUTE; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::vector<VkDescriptorSetLayout> m_set_layouts{};

    std::shared_ptr<VulkanShader> m_shader;
    ShaderGroup m_shader_group;

    void create_pipeline_layout();
};

} // namespace Mizu::Vulkan