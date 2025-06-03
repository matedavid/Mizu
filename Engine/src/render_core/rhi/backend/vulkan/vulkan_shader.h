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

class VulkanShaderBase : public virtual IShader
{
  public:
    virtual ~VulkanShaderBase();

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override;
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override;
    [[nodiscard]] std::optional<VkShaderStageFlags> get_property_stage(std::string_view name) const;

    [[nodiscard]] std::vector<ShaderConstant> get_constants() const override;
    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override;
    [[nodiscard]] std::optional<VkShaderStageFlags> get_constant_stage(std::string_view name) const;

    [[nodiscard]] std::vector<ShaderProperty> get_properties_in_set(uint32_t set) const;

    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const { return m_pipeline_layout; }

    [[nodiscard]] std::optional<VkDescriptorSetLayout> get_descriptor_set_layout(uint32_t set) const
    {
        if (set >= m_descriptor_set_layouts.size())
            return std::nullopt;

        return m_descriptor_set_layouts[set];
    }

    static VkDescriptorType get_vulkan_descriptor_type(const ShaderPropertyT& value);

  protected:
    std::unordered_map<std::string, ShaderProperty> m_properties;
    std::unordered_map<std::string, ShaderConstant> m_constants;
    std::vector<ShaderOutput> m_outputs;

    // Property and Constant to shader stage
    std::unordered_map<std::string, VkShaderStageFlags> m_uniform_to_stage;

    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    std::vector<VkPushConstantRange> m_push_constant_ranges;

    void create_descriptor_set_layouts();
    void create_push_constant_ranges();
    void create_pipeline_layout();
};

class VulkanGraphicsShader : public GraphicsShader, public VulkanShaderBase
{
  public:
    VulkanGraphicsShader(const ShaderStageInfo& vert_info, const ShaderStageInfo& frag_info);
    ~VulkanGraphicsShader() override;

    [[nodiscard]] std::vector<ShaderOutput> get_outputs() const override;

    [[nodiscard]] VkPipelineShaderStageCreateInfo get_vertex_stage_create_info() const;
    [[nodiscard]] VkPipelineShaderStageCreateInfo get_fragment_stage_create_info() const;

    [[nodiscard]] VkVertexInputBindingDescription get_vertex_input_binding_description() const
    {
        return m_vertex_input_binding_description;
    }
    [[nodiscard]] std::vector<VkVertexInputAttributeDescription> get_vertex_input_attribute_descriptions() const
    {
        return m_vertex_input_attribute_descriptions;
    }

  private:
    VkShaderModule m_vertex_module{VK_NULL_HANDLE};
    std::string m_vertex_entry_point;

    VkShaderModule m_fragment_module{VK_NULL_HANDLE};
    std::string m_fragment_entry_point;

    VkVertexInputBindingDescription m_vertex_input_binding_description{};
    std::vector<VkVertexInputAttributeDescription> m_vertex_input_attribute_descriptions;

    void retrieve_vertex_input_info(const ShaderReflection& reflection);
    void retrieve_shader_properties_info(const ShaderReflection& vertex_reflection,
                                         const ShaderReflection& fragment_reflection);
    void retrieve_shader_constants_info(const ShaderReflection& vertex_reflection,
                                        const ShaderReflection& fragment_reflection);
    void retrieve_shader_outputs_info(const ShaderReflection& fragment_reflection);
};

class VulkanComputeShader : public ComputeShader, public VulkanShaderBase
{
  public:
    VulkanComputeShader(const ShaderStageInfo& comp_info);
    ~VulkanComputeShader() override;

    [[nodiscard]] VkPipelineShaderStageCreateInfo get_stage_create_info() const;

  private:
    VkShaderModule m_module{VK_NULL_HANDLE};
    std::string m_entry_point;

    void retrieve_shader_properties_info(const ShaderReflection& reflection);
    void retrieve_shader_constants_info(const ShaderReflection& reflection);
};

} // namespace Mizu::Vulkan
