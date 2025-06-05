#include "vulkan_compute_pipeline.h"

#include "vk_core.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

#include "utility/assert.h"

namespace Mizu::Vulkan
{

VulkanComputePipeline::VulkanComputePipeline(const Description& desc)
{
    m_shader = std::dynamic_pointer_cast<VulkanShader>(desc.shader);
    MIZU_ASSERT(m_shader != nullptr && m_shader->get_type() == ShaderType::Compute,
                "No compute shader provided in ComputePipeline");

    create_pipeline_layout();

    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.stage = m_shader->get_stage_create_info();
    create_info.layout = m_pipeline_layout;

    VK_CHECK(vkCreateComputePipelines(
        VulkanContext.device->handle(), VK_NULL_HANDLE, 1, &create_info, nullptr, &m_pipeline));
}

VulkanComputePipeline::~VulkanComputePipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(VulkanContext.device->handle(), m_pipeline_layout, nullptr);
}

void VulkanComputePipeline::create_pipeline_layout()
{
    // Gather resources

    const uint32_t max_descriptor_set = Renderer::get_capabilities().max_resource_group_sets;

    std::vector<std::vector<ShaderProperty>> properties_per_set;
    std::vector<ShaderConstant> constants;
    std::unordered_map<std::string, VkShaderStageFlags> resource_to_shader_stages;

    const auto get_properties_constants = [&](const VulkanShader& shader) {
        for (const ShaderProperty& property : shader.get_properties())
        {
            MIZU_ASSERT(property.binding_info.set < max_descriptor_set,
                        "Property set is bigger or equal than max descriptor set ({} >= {})",
                        property.binding_info.set,
                        max_descriptor_set);

            if (property.binding_info.set >= properties_per_set.size())
            {
                for (size_t i = properties_per_set.size(); i < property.binding_info.set + 1; ++i)
                    properties_per_set.emplace_back();
            }

            const VkShaderStageFlags stage_flags = VulkanShader::get_vulkan_shader_type(shader.get_type());
            auto [it, inserted] = resource_to_shader_stages.try_emplace(property.name, stage_flags);

            if (!inserted)
            {
                it->second |= stage_flags;
            }
            else
            {
                properties_per_set[property.binding_info.set].push_back(property);
            }
        }

        for (const ShaderConstant& constant : shader.get_constants())
        {
            const VkShaderStageFlags stage_flags = VulkanShader::get_vulkan_shader_type(shader.get_type());
            auto [it, inserted] = resource_to_shader_stages.try_emplace(constant.name, stage_flags);

            if (!inserted)
            {
                it->second |= stage_flags;
            }
            else
            {
                constants.push_back(constant);
            }
        }
    };

    get_properties_constants(*m_shader);

    // Create pipeline layout

    m_set_layouts.clear();
    std::vector<VkPushConstantRange> push_constant_ranges;

    for (const std::vector<ShaderProperty>& properties : properties_per_set)
    {
        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        layout_bindings.reserve(properties.size());

        for (const ShaderProperty& property : properties)
        {
            VkDescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = property.binding_info.binding;
            layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(property.value);
            layout_binding.descriptorCount = 1;
            layout_binding.stageFlags = resource_to_shader_stages[property.name];
            layout_binding.pImmutableSamplers = nullptr;

            layout_bindings.push_back(layout_binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_create_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
        layout_create_info.pBindings = layout_bindings.data();

        VkDescriptorSetLayout layout = VulkanContext.layout_cache->create_descriptor_layout(layout_create_info);
        m_set_layouts.push_back(layout);
    }

    for (const ShaderConstant& constant : constants)
    {
        VkPushConstantRange range{};
        range.stageFlags = resource_to_shader_stages[constant.name];
        range.offset = 0;
        range.size = constant.size;

        push_constant_ranges.push_back(range);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(m_set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = m_set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

    VK_CHECK(vkCreatePipelineLayout(
        VulkanContext.device->handle(), &pipeline_layout_create_info, nullptr, &m_pipeline_layout));
}

} // namespace Mizu::Vulkan