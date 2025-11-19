#include "vulkan_pipeline.h"

#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu::Vulkan
{

void create_pipeline_layout(
    const ShaderGroup& shader_group,
    VkPipelineLayout& pipeline_layout,
    std::vector<VkDescriptorSetLayout>& set_layouts)
{
    std::vector<VkPushConstantRange> push_constant_ranges;

    for (uint32_t set = 0; set < shader_group.get_max_set(); ++set)
    {
        const std::vector<ShaderResource>& parameters = shader_group.get_parameters_in_set(set);

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        layout_bindings.reserve(parameters.size());

        for (const ShaderResource& parameter : parameters)
        {
            VkDescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = parameter.binding_info.binding;
            layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(parameter.value);
            layout_binding.descriptorCount = 1;
            layout_binding.stageFlags =
                VulkanShader::get_vulkan_shader_stage_bits(shader_group.get_resource_stage_bits(parameter.name));
            layout_binding.pImmutableSamplers = nullptr;

            layout_bindings.push_back(layout_binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_create_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
        layout_create_info.pBindings = layout_bindings.data();

        VkDescriptorSetLayout layout = VulkanContext.layout_cache->create_descriptor_layout(layout_create_info);
        set_layouts.push_back(layout);
    }

    for (const ShaderPushConstant& constant : shader_group.get_constants())
    {
        VkPushConstantRange range{};
        range.stageFlags =
            VulkanShader::get_vulkan_shader_stage_bits(shader_group.get_resource_stage_bits(constant.name));
        range.offset = 0;
        range.size = static_cast<uint32_t>(constant.size);

        push_constant_ranges.push_back(range);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

    VK_CHECK(vkCreatePipelineLayout(
        VulkanContext.device->handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
}

} // namespace Mizu::Vulkan