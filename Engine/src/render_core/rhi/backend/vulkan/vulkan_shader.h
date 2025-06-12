#pragma once

#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/shader.h"

namespace Mizu
{
class ShaderReflection;
}

namespace Mizu::Vulkan
{

class VulkanShader : public Shader
{
  public:
    VulkanShader(Description desc);
    ~VulkanShader() override;

    VkPipelineShaderStageCreateInfo get_stage_create_info() const;

    static VkShaderStageFlagBits get_vulkan_shader_type(ShaderType type);
    static VkShaderStageFlags get_vulkan_shader_stage_bits(ShaderType stage);
    static VkDescriptorType get_vulkan_descriptor_type(const ShaderPropertyT& value);

    std::string get_entry_point() const override { return m_description.entry_point; }
    ShaderType get_type() const override { return m_description.type; }

    const std::vector<ShaderProperty>& get_properties() const override;
    const std::vector<ShaderConstant>& get_constants() const override;
    const std::vector<ShaderInput>& get_inputs() const override;
    const std::vector<ShaderOutput>& get_outputs() const override;

    VkShaderModule handle() const { return m_handle; }

  private:
    VkShaderModule m_handle{VK_NULL_HANDLE};

    Description m_description{};
    std::unique_ptr<ShaderReflection> m_reflection{};
};

} // namespace Mizu::Vulkan
