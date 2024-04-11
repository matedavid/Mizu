#include "vulkan_shader.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <ranges>
#include <spirv_reflect.h>

#include "backend/vulkan/vk_core.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_device.h"
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
    }
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

    // Cleanup reflection
    spvReflectDestroyShaderModule(&vertex_reflect_module);
    spvReflectDestroyShaderModule(&fragment_reflect_module);
}

VulkanShader::~VulkanShader() {
    vkDestroyShaderModule(VulkanContext.device->handle(), m_vertex_module, nullptr);
    vkDestroyShaderModule(VulkanContext.device->handle(), m_fragment_module, nullptr);
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

    // Cleanup reflection
    spvReflectDestroyShaderModule(&reflect_module);
}

VulkanComputeShader::~VulkanComputeShader() {
    vkDestroyShaderModule(VulkanContext.device->handle(), m_module, nullptr);
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
