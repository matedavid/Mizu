#include "vulkan_shader.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <glm/glm.hpp>
#include <ranges>
#include <spirv_reflect.h>

#include "backend/vulkan/vk_core.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_utils.h"

namespace Mizu::Vulkan {

#define SPIRV_REFLECT_CHECK(expression)                   \
    do {                                                  \
        if (expression != SPV_REFLECT_RESULT_SUCCESS) {   \
            assert(false && "Spirv-reflect call failed"); \
        }                                                 \
    } while (false)

//
// VulkanShaderBase
//

VulkanShaderBase::~VulkanShaderBase() {
    for (const auto& layout : m_descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(VulkanContext.device->handle(), layout, nullptr);
    }

    vkDestroyPipelineLayout(VulkanContext.device->handle(), m_pipeline_layout, nullptr);
}

std::vector<char> VulkanShaderBase::read_shader_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    assert(file.is_open() && "Failed to open shader file");

    const auto size = static_cast<uint32_t>(file.tellg());
    std::vector<char> content(size);

    file.seekg(0);
    file.read(content.data(), size);

    file.close();

    return content;
}

std::optional<VulkanDescriptorInfo> VulkanShaderBase::get_descriptor_info(std::string_view name) const {
    const auto it = m_descriptor_info.find(std::string{name});
    if (it == m_descriptor_info.end())
        return std::nullopt;

    return it->second;
}

std::vector<VulkanDescriptorInfo> VulkanShaderBase::get_descriptors_in_set(uint32_t set) const {
    std::vector<VulkanDescriptorInfo> set_descriptors;

    for (const auto& [_, descriptor] : m_descriptor_info) {
        if (descriptor.set == set)
            set_descriptors.push_back(descriptor);
    }

    return set_descriptors;
}

std::optional<VulkanPushConstantInfo> VulkanShaderBase::get_push_constant_info(std::string_view name) const {
    const auto it = m_push_constant_info.find(std::string{name});
    if (it == m_push_constant_info.end())
        return std::nullopt;

    return it->second;
}

void VulkanShaderBase::create_pipeline_layout() {
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(m_descriptor_set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = m_descriptor_set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(m_push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = m_push_constant_ranges.data();

    VK_CHECK(vkCreatePipelineLayout(
        VulkanContext.device->handle(), &pipeline_layout_create_info, nullptr, &m_pipeline_layout));
}

void VulkanShaderBase::retrieve_set_bindings(const std::vector<SpvReflectDescriptorSet*>& descriptor_sets,
                                             VkShaderStageFlags stage,
                                             SetBindingsT& set_bindings) {
    for (const auto& set_info : descriptor_sets) {
        for (uint32_t i = 0; i < set_info->binding_count; ++i) {
            auto* set_binding = set_info->bindings[i];

            VkDescriptorSetLayoutBinding binding{};
            binding.binding = set_binding->binding;
            binding.descriptorType = static_cast<VkDescriptorType>(set_binding->descriptor_type);
            binding.descriptorCount = set_binding->count;
            binding.stageFlags = stage;
            binding.pImmutableSamplers = VK_NULL_HANDLE;

            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                // Don't know why, but it's done this way in the example
                // (https://github.com/KhronosGroup/SPIRV-Reflect/blob/master/examples/main_descriptors.cpp)
                for (uint32_t i_dim = 0; i_dim < set_binding->array.dims_count; ++i_dim)
                    binding.descriptorCount *= set_binding->array.dims[i_dim];
            }

            if (!set_bindings.contains(set_info->set))
                set_bindings.insert({set_info->set, {}});
            set_bindings[set_info->set].push_back(binding);

            // Reflection
            VulkanDescriptorInfo descriptor_info{};
            descriptor_info.name = std::string(set_binding->name);
            descriptor_info.type = static_cast<VkDescriptorType>(set_binding->descriptor_type);
            descriptor_info.stage = stage;
            descriptor_info.set = set_binding->set;
            descriptor_info.binding = set_binding->binding;
            descriptor_info.size = set_binding->block.size;
            descriptor_info.count = set_binding->count;

            for (uint32_t j = 0; j < set_binding->block.member_count; ++j) {
                const auto mem = set_binding->block.members[j];

                const VulkanUniformBufferMember member = {
                    .name = mem.name,
                    .size = mem.size,
                    .padded_size = mem.padded_size,
                    .offset = mem.offset,
                };
                descriptor_info.uniform_buffer_members.push_back(member);
            }

            m_descriptor_info.insert({descriptor_info.name, descriptor_info});
        }
    }
}

void VulkanShaderBase::create_descriptor_set_layouts(const SetBindingsT& set_bindings) {
    if (set_bindings.empty())
        return;

    const uint32_t biggest_set = (--set_bindings.end())->first;

    m_descriptor_set_layouts.resize(biggest_set + 1);

    for (uint32_t set = 0; set <= biggest_set; ++set) {
        VkDescriptorSetLayoutCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.bindingCount = 0;
        create_info.pBindings = nullptr;

        const auto it = set_bindings.find(set);
        if (it != set_bindings.end()) {
            create_info.bindingCount = static_cast<uint32_t>(it->second.size());
            create_info.pBindings = it->second.data();
        }

        VK_CHECK(vkCreateDescriptorSetLayout(
            VulkanContext.device->handle(), &create_info, nullptr, &m_descriptor_set_layouts[set]));
    }
}
void VulkanShaderBase::retrieve_push_constant_ranges(const SpvReflectShaderModule& module, VkShaderStageFlags stage) {
    uint32_t push_constant_count;
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, nullptr));

    std::vector<SpvReflectBlockVariable*> push_constant_blocks(push_constant_count);
    SPIRV_REFLECT_CHECK(
        spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, push_constant_blocks.data()));

    for (const auto& push_constant_block : push_constant_blocks) {
        VkPushConstantRange range{};
        range.offset = push_constant_block->offset;
        range.size = push_constant_block->size;
        range.stageFlags = stage;

        m_push_constant_ranges.push_back(range);

        // Reflection
        VulkanPushConstantInfo push_constant_info{};
        push_constant_info.name = push_constant_block->name;
        push_constant_info.size = push_constant_block->size;
        push_constant_info.stage = stage;

        m_push_constant_info.insert({push_constant_block->name, push_constant_info});
    }
}

std::vector<ShaderProperty> VulkanShaderBase::get_properties_internal() const {
    std::vector<ShaderProperty> properties;

    for (const auto& info : m_descriptor_info | std::views::values) {
        properties.push_back(*get_property_internal(info.name));
    }

    return properties;
}

std::optional<ShaderProperty> VulkanShaderBase::get_property_internal(std::string_view name) const {
    auto get_type = [](const VulkanUniformBufferMember& member) -> ShaderValueProperty::Type {
        if (member.size == sizeof(float))
            return ShaderValueProperty::Type::Float;
        else if (member.size == sizeof(glm::vec2))
            return ShaderValueProperty::Type::Vec2;
        else if (member.size == sizeof(glm::vec3))
            return ShaderValueProperty::Type::Vec3;
        else if (member.size == sizeof(glm::vec4))
            return ShaderValueProperty::Type::Vec4;
        else if (member.size == sizeof(glm::mat3))
            return ShaderValueProperty::Type::Mat3;
        else if (member.size == sizeof(glm::mat4))
            return ShaderValueProperty::Type::Mat4;

        return ShaderValueProperty::Type::Custom;
    };

    const auto it = m_descriptor_info.find(std::string{name});
    if (it == m_descriptor_info.end())
        return std::nullopt;

    // TODO: Should probably separate VK_DESCRIPTOR_TYPE_STORAGE_IMAGE into a different type or create
    // more general type instead of only ShaderTextureProperty
    if (it->second.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        || it->second.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        return ShaderTextureProperty{.name = std::string{name}};
    } else if (it->second.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        auto prop = ShaderUniformBufferProperty{};
        prop.name = name;
        prop.total_size = 0;

        for (const auto& member : it->second.uniform_buffer_members) {
            prop.members.push_back(ShaderValueProperty{
                .type = get_type(member),
                .size = member.size,
                .name = member.name,
            });

            prop.total_size += member.padded_size;
        }

        return prop;
    }

    return std::nullopt;
}

std::optional<ShaderConstant> VulkanShaderBase::get_constant_internal(std::string_view name) const {
    const auto info = get_push_constant_info(name);
    if (!info.has_value())
        return std::nullopt;

    return ShaderConstant{
        .name = info->name,
        .size = info->size,
    };
}

//
// VulkanShader
//

VulkanShader::VulkanShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path) {
    assert(std::filesystem::exists(vertex_path) && "Vertex shader path does not exist");
    assert(std::filesystem::exists(fragment_path) && "Fragment shader path does not exist");

    const auto vertex_src = read_shader_file(vertex_path);
    const auto fragment_src = read_shader_file(fragment_path);

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
    SpvReflectShaderModule vertex_reflect_module, fragment_reflect_module;
    SPIRV_REFLECT_CHECK(spvReflectCreateShaderModule(
        vertex_src.size(), reinterpret_cast<const uint32_t*>(vertex_src.data()), &vertex_reflect_module));
    SPIRV_REFLECT_CHECK(spvReflectCreateShaderModule(
        fragment_src.size(), reinterpret_cast<const uint32_t*>(fragment_src.data()), &fragment_reflect_module));

    assert(static_cast<VkShaderStageFlagBits>(vertex_reflect_module.shader_stage) == VK_SHADER_STAGE_VERTEX_BIT
           && "Vertex stage does not match");
    assert(static_cast<VkShaderStageFlagBits>(fragment_reflect_module.shader_stage) == VK_SHADER_STAGE_FRAGMENT_BIT
           && "Fragment stage does not match");

    retrieve_vertex_input_info(vertex_reflect_module);

    retrieve_descriptor_set_info(vertex_reflect_module, fragment_reflect_module);
    retrieve_push_constants_info(vertex_reflect_module, fragment_reflect_module);

    create_pipeline_layout();

    // Cleanup reflection
    spvReflectDestroyShaderModule(&vertex_reflect_module);
    spvReflectDestroyShaderModule(&fragment_reflect_module);
}

VulkanShader::~VulkanShader() {
    vkDestroyShaderModule(VulkanContext.device->handle(), m_vertex_module, nullptr);
    vkDestroyShaderModule(VulkanContext.device->handle(), m_fragment_module, nullptr);
}

VkPipelineShaderStageCreateInfo VulkanShader::get_vertex_stage_create_info() const {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage.module = m_vertex_module;
    stage.pName = "main";

    return stage;
}

VkPipelineShaderStageCreateInfo VulkanShader::get_fragment_stage_create_info() const {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage.module = m_fragment_module;
    stage.pName = "main";

    return stage;
}

std::vector<ShaderProperty> VulkanShader::get_properties() const {
    return get_properties_internal();
}

std::optional<ShaderProperty> VulkanShader::get_property(std::string_view name) const {
    return get_property_internal(name);
}

std::optional<ShaderConstant> VulkanShader::get_constant(std::string_view name) const {
    return get_constant_internal(name);
}

void VulkanShader::retrieve_vertex_input_info(const SpvReflectShaderModule& module) {
    uint32_t count;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&module, &count, nullptr));

    std::vector<SpvReflectInterfaceVariable*> input_variables{count};
    SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&module, &count, input_variables.data()));

    const auto is_not_built_in = [](const SpvReflectInterfaceVariable* variable) {
        return static_cast<uint32_t>(variable->built_in) == std::numeric_limits<uint32_t>::max();
    };

    std::vector<SpvReflectInterfaceVariable*> non_builtin_variables;
    std::ranges::copy_if(input_variables, std::back_inserter(non_builtin_variables), is_not_built_in);
    if (non_builtin_variables.empty())
        return;

    std::ranges::sort(non_builtin_variables, [](SpvReflectInterfaceVariable* a, SpvReflectInterfaceVariable* b) {
        return a->location < b->location;
    });

    m_vertex_input_binding_description = VkVertexInputBindingDescription{};
    m_vertex_input_binding_description.binding = 0;
    m_vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    uint32_t stride = 0;
    for (const auto* input_var : non_builtin_variables) {
        const auto format = static_cast<VkFormat>(input_var->format);

        VkVertexInputAttributeDescription description{};
        description.binding = 0;
        description.location = input_var->location;
        description.format = format;
        description.offset = stride;

        m_vertex_input_attribute_descriptions.push_back(description);

        stride += VulkanUtils::get_format_size(format);
    }

    m_vertex_input_binding_description.stride = stride;
}

void VulkanShader::retrieve_descriptor_set_info(const SpvReflectShaderModule& vertex_module,
                                                const SpvReflectShaderModule& fragment_module) {
    SetBindingsT set_bindings;

    // Vertex descriptor sets
    {
        uint32_t set_count;
        SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&vertex_module, &set_count, nullptr));

        std::vector<SpvReflectDescriptorSet*> descriptor_sets(set_count);
        SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&vertex_module, &set_count, descriptor_sets.data()));

        retrieve_set_bindings(descriptor_sets, VK_SHADER_STAGE_VERTEX_BIT, set_bindings);
    }

    // Fragment descriptor sets
    {
        uint32_t set_count;
        SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&fragment_module, &set_count, nullptr));

        std::vector<SpvReflectDescriptorSet*> descriptor_sets(set_count);
        SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&fragment_module, &set_count, descriptor_sets.data()));

        retrieve_set_bindings(descriptor_sets, VK_SHADER_STAGE_FRAGMENT_BIT, set_bindings);
    }

    create_descriptor_set_layouts(set_bindings);
}

void VulkanShader::retrieve_push_constants_info(const SpvReflectShaderModule& vertex_module,
                                                const SpvReflectShaderModule& fragment_module) {
    retrieve_push_constant_ranges(vertex_module, VK_SHADER_STAGE_VERTEX_BIT);
    retrieve_push_constant_ranges(fragment_module, VK_SHADER_STAGE_FRAGMENT_BIT);
}

//
// VulkanComputeShader
//

VulkanComputeShader::VulkanComputeShader(const std::filesystem::path& path) {
    assert(std::filesystem::exists(path) && "Compute shader path does not exist");

    const auto src = read_shader_file(path);

    VkShaderModuleCreateInfo vertex_create_info{};
    vertex_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_create_info.codeSize = src.size();
    vertex_create_info.pCode = reinterpret_cast<const uint32_t*>(src.data());

    VK_CHECK(vkCreateShaderModule(VulkanContext.device->handle(), &vertex_create_info, nullptr, &m_module));

    // Reflection
    SpvReflectShaderModule reflect_module;
    SPIRV_REFLECT_CHECK(
        spvReflectCreateShaderModule(src.size(), reinterpret_cast<const uint32_t*>(src.data()), &reflect_module));

    assert(static_cast<VkShaderStageFlagBits>(reflect_module.shader_stage) == VK_SHADER_STAGE_COMPUTE_BIT
           && "Compute stage does not match");

    retrieve_descriptor_set_info(reflect_module);
    retrieve_push_constants_info(reflect_module);

    create_pipeline_layout();

    // Cleanup reflection
    spvReflectDestroyShaderModule(&reflect_module);
}

VulkanComputeShader::~VulkanComputeShader() {
    vkDestroyShaderModule(VulkanContext.device->handle(), m_module, nullptr);
}

std::vector<ShaderProperty> VulkanComputeShader::get_properties() const {
    return get_properties_internal();
}

std::optional<ShaderProperty> VulkanComputeShader::get_property(std::string_view name) const {
    return get_property_internal(name);
}

std::optional<ShaderConstant> VulkanComputeShader::get_constant(std::string_view name) const {
    return get_constant_internal(name);
}

void VulkanComputeShader::retrieve_descriptor_set_info(const SpvReflectShaderModule& module) {
    SetBindingsT set_bindings;

    uint32_t set_count;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&module, &set_count, nullptr));

    std::vector<SpvReflectDescriptorSet*> descriptor_sets(set_count);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&module, &set_count, descriptor_sets.data()));

    retrieve_set_bindings(descriptor_sets, VK_SHADER_STAGE_COMPUTE_BIT, set_bindings);

    create_descriptor_set_layouts(set_bindings);
}

void VulkanComputeShader::retrieve_push_constants_info(const SpvReflectShaderModule& module) {
    retrieve_push_constant_ranges(module, VK_SHADER_STAGE_COMPUTE_BIT);
}

} // namespace Mizu::Vulkan
