#pragma once

#include "render_core/rhi/rtx/ray_tracing_shader.h"

#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu::Vulkan
{

class VulkanRayTracingShader : public RayTracingShader, public VulkanShaderBase
{
  public:
    VulkanRayTracingShader(RayTracingShaderStageInfo info);
    ~VulkanRayTracingShader() override;

    VkPipelineShaderStageCreateInfo get_stage_create_info() const;

    VkShaderModule handle() const { return m_module; }

  private:
    VkShaderModule m_module;
    RayTracingShaderStageInfo m_stage_info;

    void retrieve_shader_properties_info(const ShaderReflection& reflection);
    void retrieve_shader_constants_info(const ShaderReflection& reflection);

    static VkShaderStageFlagBits get_vulkan_raytracing_stage(RayTracingShaderStage stage);
};

} // namespace Mizu::Vulkan