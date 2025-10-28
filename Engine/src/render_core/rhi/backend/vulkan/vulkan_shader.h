#pragma once

#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/shader.h"
#include "renderer/shader/shader_types.h"

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
    static VkDescriptorType get_vulkan_descriptor_type(const ShaderResourceT& value);

    const std::string& get_entry_point() const override { return m_description.entry_point; }
    ShaderType get_type() const override { return m_description.type; }

    const SlangReflection& get_reflection() const override;

    VkShaderModule handle() const { return m_handle; }

  private:
    VkShaderModule m_handle{VK_NULL_HANDLE};

    Description m_description{};
    std::unique_ptr<SlangReflection> m_reflection{};
};

} // namespace Mizu::Vulkan
