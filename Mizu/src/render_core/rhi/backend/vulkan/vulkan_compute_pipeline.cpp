#include "vulkan_compute_pipeline.h"

#include "vk_core.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

#include "utility/assert.h"

namespace Mizu::Vulkan
{

VulkanComputePipeline::VulkanComputePipeline(const Description& desc)
{
    m_shader = std::dynamic_pointer_cast<VulkanComputeShader>(desc.shader);
    MIZU_ASSERT(m_shader != nullptr, "Could not convert ComputeShader to VulkanComputeShader");

    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.stage = m_shader->get_stage_create_info();
    create_info.layout = m_shader->get_pipeline_layout();

    VK_CHECK(vkCreateComputePipelines(
        VulkanContext.device->handle(), VK_NULL_HANDLE, 1, &create_info, nullptr, &m_pipeline));
}

VulkanComputePipeline::~VulkanComputePipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
}

void VulkanComputePipeline::push_constant(VkCommandBuffer command_buffer,
                                          std::string_view name,
                                          uint32_t size,
                                          const void* data)
{
    const auto info = m_shader->get_constant(name);
    MIZU_ASSERT(info.has_value(), "Push constant '{}' not found in ComputePipeline", name);

    MIZU_ASSERT(info->size == size,
                "Size of provided data and size of push constant do not match ({} != {})",
                size,
                info->size);

    vkCmdPushConstants(
        command_buffer, m_shader->get_pipeline_layout(), *m_shader->get_constant_stage(name), 0, size, data);
}

} // namespace Mizu::Vulkan