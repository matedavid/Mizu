#include "vulkan_ray_tracing_shader.h"

#include "render_core/shader/shader_reflection.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"

#include "utility/filesystem.h"

namespace Mizu::Vulkan
{

VulkanRayTracingShader::VulkanRayTracingShader(RayTracingShaderStageInfo info) : m_stage_info(std::move(info))
{
    const auto source = Filesystem::read_file(m_stage_info.path);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = source.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(source.data());

    VK_CHECK(vkCreateShaderModule(VulkanContext.device->handle(), &create_info, nullptr, &m_module));

    // Reflection
    ShaderReflection reflection(source);

    retrieve_shader_properties_info(reflection);
    retrieve_shader_constants_info(reflection);

    create_pipeline_layout();
}

VulkanRayTracingShader::~VulkanRayTracingShader()
{
    vkDestroyShaderModule(VulkanContext.device->handle(), m_module, nullptr);
}

VkPipelineShaderStageCreateInfo VulkanRayTracingShader::get_stage_create_info() const
{
    VkPipelineShaderStageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_info.stage = get_vulkan_raytracing_stage(m_stage_info.stage);
    create_info.module = m_module;
    create_info.pName = m_stage_info.entry_point.c_str();

    return create_info;
}

void VulkanRayTracingShader::retrieve_shader_properties_info(const ShaderReflection& reflection)
{
    const VkShaderStageFlagBits stage_flag = get_vulkan_raytracing_stage(m_stage_info.stage);

    for (const ShaderProperty& property : reflection.get_properties())
    {
        m_properties.insert({property.name, property});

        auto [it, inserted] = m_uniform_to_stage.try_emplace(property.name, stage_flag);
        if (!inserted)
            it->second |= stage_flag;
    }

    create_descriptor_set_layouts();
}

void VulkanRayTracingShader::retrieve_shader_constants_info(const ShaderReflection& reflection)
{
    const VkShaderStageFlagBits stage_flag = get_vulkan_raytracing_stage(m_stage_info.stage);

    for (const ShaderConstant& constant : reflection.get_constants())
    {
        m_constants.insert({constant.name, constant});

        auto [it, inserted] = m_uniform_to_stage.try_emplace(constant.name, stage_flag);
        if (!inserted)
            it->second |= stage_flag;
    }

    create_push_constant_ranges();
}

VkShaderStageFlagBits VulkanRayTracingShader::get_vulkan_raytracing_stage(RayTracingShaderStage stage)
{
    switch (stage)
    {
    case RayTracingShaderStage::Raygen:
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case RayTracingShaderStage::AnyHit:
        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case RayTracingShaderStage::ClosestHit:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case RayTracingShaderStage::Miss:
        return VK_SHADER_STAGE_MISS_BIT_KHR;
    case RayTracingShaderStage::Intersection:
        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    }

    MIZU_UNREACHABLE("Not valid stage");
}

} // namespace Mizu::Vulkan