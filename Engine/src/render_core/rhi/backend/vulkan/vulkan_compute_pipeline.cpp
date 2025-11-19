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

    m_shader_group = ShaderGroup{};
    m_shader_group.add_shader(*m_shader);

    create_pipeline_layout(m_shader_group, m_pipeline_layout, m_set_layouts);

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

} // namespace Mizu::Vulkan