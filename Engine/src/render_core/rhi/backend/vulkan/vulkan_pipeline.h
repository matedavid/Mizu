#pragma once

#include <vulkan/vulkan.h>

namespace Mizu::Vulkan
{

class IVulkanPipeline
{
  public:
    virtual VkPipeline handle() const = 0;
    virtual VkPipelineLayout get_pipeline_layout() const = 0;
    virtual VkDescriptorSetLayout get_descriptor_set_layout(uint32_t set) const = 0;
    virtual const ShaderGroup& get_shader_group() const = 0;
    virtual VkPipelineBindPoint get_pipeline_bind_point() const = 0;
};

} // namespace Mizu::Vulkan