#pragma once

#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "shader.h"

// Forward declarations
struct SpvReflectShaderModule;
struct SpvReflectDescriptorSet;

namespace Mizu::Vulkan {

class VulkanShaderBase {
  public:
    virtual ~VulkanShaderBase();

    [[nodiscard]] std::vector<VkDescriptorSetLayout> get_descriptor_set_layouts() const {
        return m_descriptor_set_layouts;
    }
    [[nodiscard]] std::vector<VkPushConstantRange> get_push_constant_ranges() const { return m_push_constant_ranges; }

  protected:
    [[nodiscard]] static std::vector<char> read_shader_file(const std::filesystem::path& path);

    using SetBindingsT = std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>;
    static void retrieve_set_bindings(const std::vector<SpvReflectDescriptorSet*>& descriptor_sets,
                                      VkShaderStageFlags stage,
                                      SetBindingsT& set_bindings);
    void create_descriptor_set_layouts(const SetBindingsT& set_bindings);

    void retrieve_push_constant_ranges(const SpvReflectShaderModule& module, VkShaderStageFlags stage);

    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    std::vector<VkPushConstantRange> m_push_constant_ranges;
};

class VulkanShader : public Shader, public VulkanShaderBase {
  public:
    VulkanShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path);
    ~VulkanShader() override;

    [[nodiscard]] VkVertexInputBindingDescription get_vertex_input_binding_description() const {
        return m_vertex_input_binding_description;
    }
    [[nodiscard]] std::vector<VkVertexInputAttributeDescription> get_vertex_input_attribute_descriptions() const {
        return m_vertex_input_attribute_descriptions;
    }

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

  private:
    VkShaderModule m_module{VK_NULL_HANDLE};

    void retrieve_descriptor_set_info(const SpvReflectShaderModule& module);
    void retrieve_push_constants_info(const SpvReflectShaderModule& module);
};

} // namespace Mizu::Vulkan
