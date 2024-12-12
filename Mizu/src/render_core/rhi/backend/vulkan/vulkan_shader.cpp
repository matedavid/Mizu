#include "vulkan_shader.h"

#include <ranges>

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_utils.h"
#include "render_core/shader/shader_reflection.h"

#include "utility/assert.h"
#include "utility/filesystem.h"
#include "utility/logging.h"

namespace Mizu::Vulkan
{

//
// VulkanShaderBase
//

VulkanShaderBase::~VulkanShaderBase()
{
    vkDestroyPipelineLayout(VulkanContext.device->handle(), m_pipeline_layout, nullptr);
}

std::vector<ShaderProperty> VulkanShaderBase::get_properties() const
{
    std::vector<ShaderProperty> properties;
    properties.reserve(m_properties.size());

    for (const auto& [_, property] : m_properties)
    {
        properties.push_back(property);
    }

    return properties;
}

std::optional<ShaderProperty> VulkanShaderBase::get_property(std::string_view name) const
{
    const auto it = m_properties.find(std::string(name));
    if (it == m_properties.end())
        return std::nullopt;

    return it->second;
}

std::optional<VkShaderStageFlagBits> VulkanShaderBase::get_property_stage(std::string_view name) const
{
    const auto it = m_uniform_to_stage.find(std::string(name));
    if (it == m_uniform_to_stage.end())
        return std::nullopt;

    return it->second;
}

std::optional<ShaderConstant> VulkanShaderBase::get_constant(std::string_view name) const
{
    const auto it = m_constants.find(std::string(name));
    if (it == m_constants.end())
        return std::nullopt;

    return it->second;
}

std::optional<VkShaderStageFlagBits> VulkanShaderBase::get_constant_stage(std::string_view name) const
{
    const auto it = m_uniform_to_stage.find(std::string(name));
    if (it == m_uniform_to_stage.end())
        return std::nullopt;

    return it->second;
}

std::vector<ShaderProperty> VulkanShaderBase::get_properties_in_set(uint32_t set) const
{
    std::vector<ShaderProperty> properties;
    for (const auto& [_, property] : m_properties)
    {
        if (property.binding_info.set == set)
        {
            properties.push_back(property);
        }
    }

    return properties;
}

VkDescriptorType VulkanShaderBase::get_vulkan_descriptor_type(const ShaderPropertyT& value)
{
    if (std::holds_alternative<ShaderTextureProperty>(value))
    {
        const auto& texture_val = std::get<ShaderTextureProperty>(value);

        switch (texture_val.type)
        {
        case ShaderTextureProperty::Type::Sampled:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case ShaderTextureProperty::Type::Separate:
            // TODO: No idea
            MIZU_UNREACHABLE("Unimplemented");
            break;
        case ShaderTextureProperty::Type::Storage:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
    }
    else if (std::holds_alternative<ShaderBufferProperty>(value))
    {
        const auto buffer_val = std::get<ShaderBufferProperty>(value);

        switch (buffer_val.type)
        {
        case ShaderBufferProperty::Type::Uniform:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ShaderBufferProperty::Type::Storage:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
    }

    MIZU_UNREACHABLE("ShaderPropertyT should only have specified types in variant");
}

void VulkanShaderBase::create_descriptor_set_layouts()
{
    std::vector<std::vector<ShaderProperty>> set_properties;

    for (const auto& [_, property] : m_properties)
    {
        if (property.binding_info.set >= set_properties.size())
        {
            for (size_t i = set_properties.size(); i < property.binding_info.set + 1; ++i)
            {
                set_properties.emplace_back();
            }
        }

        set_properties[property.binding_info.set].push_back(property);
    }

    m_descriptor_set_layouts.resize(set_properties.size());

    for (size_t i = 0; i < set_properties.size(); ++i)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& property : set_properties[i])
        {
            VkDescriptorSetLayoutBinding binding;
            binding.binding = property.binding_info.binding;
            binding.descriptorType = get_vulkan_descriptor_type(property.value);
            binding.descriptorCount = 1;
            binding.stageFlags = m_uniform_to_stage[property.name];
            binding.pImmutableSamplers = nullptr;

            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.bindingCount = static_cast<uint32_t>(bindings.size());
        create_info.pBindings = bindings.data();

        m_descriptor_set_layouts[i] = VulkanContext.layout_cache->create_descriptor_layout(create_info);
    }
}

void VulkanShaderBase::create_push_constant_ranges()
{
    for (const auto& [_, constant] : m_constants)
    {
        VkPushConstantRange constant_range;
        constant_range.stageFlags = m_uniform_to_stage[constant.name];
        constant_range.offset = 0;
        constant_range.size = constant.size;

        m_push_constant_ranges.push_back(constant_range);
    }
}

void VulkanShaderBase::create_pipeline_layout()
{
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(m_descriptor_set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = m_descriptor_set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(m_push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = m_push_constant_ranges.data();

    VK_CHECK(vkCreatePipelineLayout(
        VulkanContext.device->handle(), &pipeline_layout_create_info, nullptr, &m_pipeline_layout));
}

//
// VulkanGraphicsShader
//

VulkanGraphicsShader::VulkanGraphicsShader(const ShaderStageInfo& vert_info, const ShaderStageInfo& frag_info)
    : m_vertex_entry_point(vert_info.entry_point)
    , m_fragment_entry_point(frag_info.entry_point)
{
    const auto vertex_src = Filesystem::read_file(vert_info.path);
    const auto fragment_src = Filesystem::read_file(frag_info.path);

    VkShaderModuleCreateInfo vertex_create_info{};
    vertex_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_create_info.codeSize = vertex_src.size();
    vertex_create_info.pCode = reinterpret_cast<const uint32_t*>(vertex_src.data());

    VkShaderModuleCreateInfo fragment_create_info{};
    fragment_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_create_info.codeSize = fragment_src.size();
    fragment_create_info.pCode = reinterpret_cast<const uint32_t*>(fragment_src.data());

    VK_CHECK(vkCreateShaderModule(VulkanContext.device->handle(), &vertex_create_info, nullptr, &m_vertex_module));
    VK_CHECK(vkCreateShaderModule(VulkanContext.device->handle(), &fragment_create_info, nullptr, &m_fragment_module));

    // Reflection
    ShaderReflection vertex_reflection(vertex_src);
    ShaderReflection fragment_reflection(fragment_src);

    retrieve_vertex_input_info(vertex_reflection);

    retrieve_shader_properties_info(vertex_reflection, fragment_reflection);
    retrieve_shader_constants_info(vertex_reflection, fragment_reflection);

    create_pipeline_layout();
}

VulkanGraphicsShader::~VulkanGraphicsShader()
{
    vkDestroyShaderModule(VulkanContext.device->handle(), m_vertex_module, nullptr);
    vkDestroyShaderModule(VulkanContext.device->handle(), m_fragment_module, nullptr);
}

VkPipelineShaderStageCreateInfo VulkanGraphicsShader::get_vertex_stage_create_info() const
{
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage.module = m_vertex_module;
    stage.pName = m_vertex_entry_point.c_str();

    return stage;
}

VkPipelineShaderStageCreateInfo VulkanGraphicsShader::get_fragment_stage_create_info() const
{
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage.module = m_fragment_module;
    stage.pName = m_fragment_entry_point.c_str();

    return stage;
}

void VulkanGraphicsShader::retrieve_vertex_input_info(const ShaderReflection& reflection)
{
    m_vertex_input_binding_description = VkVertexInputBindingDescription{};
    m_vertex_input_binding_description.binding = 0;
    m_vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    const auto shader_type_to_vk_format = [](ShaderType type) -> VkFormat {
        switch (type)
        {
        case ShaderType::Float:
            return VK_FORMAT_R32_SFLOAT;
        case ShaderType::Float2:
            return VK_FORMAT_R32G32_SFLOAT;
        case ShaderType::Float3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case ShaderType::Float4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    };

    uint32_t stride = 0;
    for (const auto& input_var : reflection.get_inputs())
    {
        VkVertexInputAttributeDescription description{};
        description.binding = 0;
        description.location = input_var.location;
        description.format = shader_type_to_vk_format(input_var.type);
        description.offset = stride;

        if (description.format == VK_FORMAT_UNDEFINED)
        {
            MIZU_ASSERT(false, "Shader Type not valid as VkFormat");
            continue;
        }

        m_vertex_input_attribute_descriptions.push_back(description);

        stride += ShaderType::size(input_var.type);
    }

    m_vertex_input_binding_description.stride = stride;
}

void VulkanGraphicsShader::retrieve_shader_properties_info(const ShaderReflection& vertex_reflection,
                                                           const ShaderReflection& fragment_reflection)
{
    // vertex reflection
    for (const auto& property : vertex_reflection.get_properties())
    {
        m_properties.insert({property.name, property});
        m_uniform_to_stage.insert({property.name, VK_SHADER_STAGE_VERTEX_BIT});
    }

    // fragment reflection
    for (const auto& property : fragment_reflection.get_properties())
    {
        m_properties.insert({property.name, property});
        m_uniform_to_stage.insert({property.name, VK_SHADER_STAGE_FRAGMENT_BIT});
    }

    create_descriptor_set_layouts();
}

void VulkanGraphicsShader::retrieve_shader_constants_info(const ShaderReflection& vertex_reflection,
                                                          const ShaderReflection& fragment_reflection)
{
    // vertex reflection
    for (const auto& constant : vertex_reflection.get_constants())
    {
        m_constants.insert({constant.name, constant});
        m_uniform_to_stage.insert({constant.name, VK_SHADER_STAGE_VERTEX_BIT});
    }

    // fragment reflection
    for (const auto& constant : fragment_reflection.get_constants())
    {
        m_constants.insert({constant.name, constant});
        m_uniform_to_stage.insert({constant.name, VK_SHADER_STAGE_FRAGMENT_BIT});
    }

    create_push_constant_ranges();
}

//
// VulkanComputeShader
//

VulkanComputeShader::VulkanComputeShader(const ShaderStageInfo& comp_info) : m_entry_point(comp_info.entry_point)
{
    const auto compute_src = Filesystem::read_file(comp_info.path);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = compute_src.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(compute_src.data());

    VK_CHECK(vkCreateShaderModule(VulkanContext.device->handle(), &create_info, nullptr, &m_module));

    // Reflection
    ShaderReflection reflection(compute_src);

    retrieve_shader_properties_info(reflection);
    retrieve_shader_constants_info(reflection);

    create_pipeline_layout();
}

VulkanComputeShader::~VulkanComputeShader()
{
    vkDestroyShaderModule(VulkanContext.device->handle(), m_module, nullptr);
}

VkPipelineShaderStageCreateInfo VulkanComputeShader::get_stage_create_info() const
{
    VkPipelineShaderStageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    create_info.module = m_module;
    create_info.pName = m_entry_point.c_str();

    return create_info;
}

void VulkanComputeShader::retrieve_shader_properties_info(const ShaderReflection& reflection)
{
    for (const auto& property : reflection.get_properties())
    {
        m_properties.insert({property.name, property});
        m_uniform_to_stage.insert({property.name, VK_SHADER_STAGE_COMPUTE_BIT});
    }

    create_descriptor_set_layouts();
}

void VulkanComputeShader::retrieve_shader_constants_info(const ShaderReflection& reflection)
{
    for (const auto& constant : reflection.get_constants())
    {
        m_constants.insert({constant.name, constant});
        m_uniform_to_stage.insert({constant.name, VK_SHADER_STAGE_COMPUTE_BIT});
    }

    create_push_constant_ranges();
}

} // namespace Mizu::Vulkan
