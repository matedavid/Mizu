#pragma once

#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/shader.h"

namespace Mizu {
class ShaderReflection;
}

namespace Mizu::Vulkan {

class VulkanShaderBase {
  public:
    virtual ~VulkanShaderBase();

    [[nodiscard]] std::vector<ShaderProperty> get_properties_base() const;
    [[nodiscard]] std::optional<ShaderProperty> get_property_base(std::string_view name) const;
    [[nodiscard]] std::optional<VkShaderStageFlagBits> get_property_stage(std::string_view name) const;

    [[nodiscard]] std::optional<ShaderConstant> get_constant_base(std::string_view name) const;
    [[nodiscard]] std::optional<VkShaderStageFlagBits> get_constant_stage(std::string_view name) const;

    [[nodiscard]] std::vector<ShaderProperty> get_properties_in_set(uint32_t set) const;

    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const { return m_pipeline_layout; }

    [[nodiscard]] std::optional<VkDescriptorSetLayout> get_descriptor_set_layout(uint32_t set) {
        if (set >= m_descriptor_set_layouts.size())
            return std::nullopt;

        return m_descriptor_set_layouts[set];
    }

    static VkDescriptorType get_vulkan_descriptor_type(const ShaderPropertyT& value);

  protected:
    std::unordered_map<std::string, ShaderProperty> m_properties;
    std::unordered_map<std::string, ShaderConstant> m_constants;

    // Property and Constant to shader stage
    std::unordered_map<std::string, VkShaderStageFlagBits> m_uniform_to_stage;

    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    std::vector<VkPushConstantRange> m_push_constant_ranges;

    void create_descriptor_set_layouts();
    void create_push_constant_ranges();
    void create_pipeline_layout();
};

class VulkanGraphicsShader : public GraphicsShader, public VulkanShaderBase {
  public:
    VulkanGraphicsShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path);
    ~VulkanGraphicsShader() override;

    [[nodiscard]] VkPipelineShaderStageCreateInfo get_vertex_stage_create_info() const;
    [[nodiscard]] VkPipelineShaderStageCreateInfo get_fragment_stage_create_info() const;

    [[nodiscard]] VkVertexInputBindingDescription get_vertex_input_binding_description() const {
        return m_vertex_input_binding_description;
    }
    [[nodiscard]] std::vector<VkVertexInputAttributeDescription> get_vertex_input_attribute_descriptions() const {
        return m_vertex_input_attribute_descriptions;
    }

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override { return get_properties_base(); }
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override {
        return get_property_base(name);
    }

    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override {
        return get_constant_base(name);
    }

  private:
    VkShaderModule m_vertex_module{VK_NULL_HANDLE};
    VkShaderModule m_fragment_module{VK_NULL_HANDLE};

    VkVertexInputBindingDescription m_vertex_input_binding_description{};
    std::vector<VkVertexInputAttributeDescription> m_vertex_input_attribute_descriptions;

    void retrieve_vertex_input_info(const ShaderReflection& reflection);
    void retrieve_shader_properties_info(const ShaderReflection& vertex_reflection,
                                         const ShaderReflection& fragment_reflection);
    void retrieve_shader_constants_info(const ShaderReflection& vertex_reflection,
                                        const ShaderReflection& fragment_reflection);
};

class VulkanComputeShader : public ComputeShader, public VulkanShaderBase {
  public:
    explicit VulkanComputeShader(const std::filesystem::path& path);
    ~VulkanComputeShader() override;

    [[nodiscard]] VkPipelineShaderStageCreateInfo get_stage_create_info() const;

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override { return get_properties_base(); }
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override {
        return get_property_base(name);
    }

    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override {
        return get_constant_base(name);
    }

  private:
    VkShaderModule m_module{VK_NULL_HANDLE};

    void retrieve_shader_properties_info(const ShaderReflection& reflection);
    void retrieve_shader_constants_info(const ShaderReflection& reflection);
};

} // namespace Mizu::Vulkan
