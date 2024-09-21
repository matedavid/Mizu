#include "vulkan_compute_pipeline.h"

#include "vk_core.h"

#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_shader.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

VulkanComputePipeline::VulkanComputePipeline(const Description& desc) {
    m_shader = std::dynamic_pointer_cast<VulkanComputeShader>(desc.shader);
    MIZU_ASSERT(m_shader != nullptr, "Could not convert ComputeShader to VulkanComputeShader");

    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.stage = m_shader->get_stage_create_info();
    create_info.layout = m_shader->get_pipeline_layout();

    VK_CHECK(vkCreateComputePipelines(
        VulkanContext.device->handle(), VK_NULL_HANDLE, 1, &create_info, nullptr, &m_pipeline));
}

} // namespace Mizu::Vulkan