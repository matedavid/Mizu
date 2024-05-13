#pragma once

#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/shader.h"

// Forward declarations
struct SpvReflectShaderModule;
struct SpvReflectDescriptorSet;

namespace Mizu::Vulkan {

struct VulkanUniformBufferMember {
    std::string name;
    uint32_t size;
    uint32_t padded_size;
    uint32_t offset;
};

struct VulkanDescriptorInfo {
    std::string name;
    VkDescriptorType type;
    VkShaderStageFlags stage;

    uint32_t set;
    uint32_t binding;
    uint32_t size;
    uint32_t count;

    std::vector<VulkanUniformBufferMember> uniform_buffer_members;
};

struct VulkanPushConstantInfo {
    std::string name;
    VkShaderStageFlags stage;
    uint32_t size;
};

class VulkanShaderBase {
  public:
    virtual ~VulkanShaderBase();

    [[nodiscard]] std::optional<VulkanDescriptorInfo> get_descriptor_info(std::string_view name) const;
    [[nodiscard]] std::vector<VulkanDescriptorInfo> get_descriptors_in_set(uint32_t set) const;

    [[nodiscard]] std::optional<VulkanPushConstantInfo> get_push_constant_info(std::string_view name) const;

    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const { return m_pipeline_layout; }

    [[nodiscard]] std::vector<VkDescriptorSetLayout> get_descriptor_set_layouts() const {
        return m_descriptor_set_layouts;
    }
    [[nodiscard]] std::vector<VkPushConstantRange> get_push_constant_ranges() const { return m_push_constant_ranges; }

    [[nodiscard]] std::optional<VkDescriptorSetLayout> get_descriptor_set_layout(uint32_t set) const;

  protected:
    void create_pipeline_layout();

    using SetBindingsT = std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>;
    void retrieve_set_bindings(const std::vector<SpvReflectDescriptorSet*>& descriptor_sets,
                               VkShaderStageFlags stage,
                               SetBindingsT& set_bindings);
    void create_descriptor_set_layouts(const SetBindingsT& set_bindings);

    void retrieve_push_constant_ranges(const SpvReflectShaderModule& module, VkShaderStageFlags stage);

    [[nodiscard]] std::vector<ShaderProperty> get_properties_internal() const;
    [[nodiscard]] std::optional<ShaderProperty> get_property_internal(std::string_view name) const;

    [[nodiscard]] std::optional<ShaderConstant> get_constant_internal(std::string_view name) const;

    // Pipeline layout
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    // Descriptor sets and push constant information
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    std::vector<VkPushConstantRange> m_push_constant_ranges;

    // Reflection information
    std::unordered_map<std::string, VulkanDescriptorInfo> m_descriptor_info;
    std::unordered_map<std::string, VulkanPushConstantInfo> m_push_constant_info;
};

class VulkanShader : public Shader, public VulkanShaderBase {
  public:
    VulkanShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path);
    ~VulkanShader() override;

    [[nodiscard]] VkPipelineShaderStageCreateInfo get_vertex_stage_create_info() const;
    [[nodiscard]] VkPipelineShaderStageCreateInfo get_fragment_stage_create_info() const;

    [[nodiscard]] VkVertexInputBindingDescription get_vertex_input_binding_description() const {
        return m_vertex_input_binding_description;
    }
    [[nodiscard]] std::vector<VkVertexInputAttributeDescription> get_vertex_input_attribute_descriptions() const {
        return m_vertex_input_attribute_descriptions;
    }

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override;
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override;

    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override;

  private:
    VkShaderModule m_vertex_module{VK_NULL_HANDLE};
    VkShaderModule m_fragment_module{VK_NULL_HANDLE};

    // Vertex input
    VkVertexInputBindingDescription m_vertex_input_binding_description{};
    std::vector<VkVertexInputAttributeDescription> m_vertex_input_attribute_descriptions;

    void retrieve_vertex_input_info(const SpvReflectShaderModule& module);

    void retrieve_descriptor_set_info(const SpvReflectShaderModule& vertex_module,
                                      const SpvReflectShaderModule& fragment_module);
    void retrieve_push_constants_info(const SpvReflectShaderModule& vertex_module,
                                      const SpvReflectShaderModule& fragment_module);
};

class VulkanComputeShader : public ComputeShader, public VulkanShaderBase {
  public:
    explicit VulkanComputeShader(const std::filesystem::path& path);
    ~VulkanComputeShader() override;

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override;
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override;

    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override;

  private:
    VkShaderModule m_module{VK_NULL_HANDLE};

    void retrieve_descriptor_set_info(const SpvReflectShaderModule& module);
    void retrieve_push_constants_info(const SpvReflectShaderModule& module);
};

} // namespace Mizu::Vulkan
