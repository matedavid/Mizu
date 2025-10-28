#include "vulkan_compute_pipeline.h"

#include "base/debug/assert.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu::Vulkan
{

VulkanComputePipeline::VulkanComputePipeline(const Description& desc)
{
    m_shader = std::dynamic_pointer_cast<VulkanShader>(desc.shader);
    MIZU_ASSERT(
        m_shader != nullptr && m_shader->get_type() == ShaderType::Compute,
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

    m_shader_group = ShaderGroup();
    m_shader_group.add_shader(*m_shader);

    // Create pipeline layout

    m_set_layouts.clear();
    std::vector<VkPushConstantRange> push_constant_ranges;

    for (uint32_t set = 0; set < m_shader_group.get_max_set(); ++set)
    {
        const std::vector<ShaderResource>& parameters = m_shader_group.get_parameters_in_set(set);

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        layout_bindings.reserve(parameters.size());

        for (const ShaderResource& parameter : parameters)
        {
            VkDescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = parameter.binding_info.binding;
            layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(parameter.value);
            layout_binding.descriptorCount = 1;
            layout_binding.stageFlags =
                VulkanShader::get_vulkan_shader_stage_bits(m_shader_group.get_resource_stage_bits(parameter.name));
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

    for (const ShaderPushConstant& constant : m_shader_group.get_constants())
    {
        VkPushConstantRange range{};
        range.stageFlags =
            VulkanShader::get_vulkan_shader_stage_bits(m_shader_group.get_resource_stage_bits(constant.name));
        range.offset = 0;
        range.size = static_cast<uint32_t>(constant.size);

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