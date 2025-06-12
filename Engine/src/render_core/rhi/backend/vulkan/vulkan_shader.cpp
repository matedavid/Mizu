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

VulkanShader::VulkanShader(Description desc) : m_description(std::move(desc))
{
    const auto source = Filesystem::read_file(m_description.path);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = source.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(source.data());

    VK_CHECK(vkCreateShaderModule(VulkanContext.device->handle(), &create_info, nullptr, &m_handle));

    m_reflection = std::make_unique<ShaderReflection>(source);
}

VulkanShader::~VulkanShader()
{
    vkDestroyShaderModule(VulkanContext.device->handle(), m_handle, nullptr);
}

VkPipelineShaderStageCreateInfo VulkanShader::get_stage_create_info() const
{
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = get_vulkan_shader_type(m_description.type);
    stage.module = m_handle;
    stage.pName = m_description.entry_point.c_str();

    return stage;
}

VkShaderStageFlagBits VulkanShader::get_vulkan_shader_type(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderType::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderType::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    case ShaderType::RtxRaygen:
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case ShaderType::RtxAnyHit:
        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case ShaderType::RtxClosestHit:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case ShaderType::RtxMiss:
        return VK_SHADER_STAGE_MISS_BIT_KHR;
    case ShaderType::RtxIntersection:
        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    }

    MIZU_UNREACHABLE("Invalid ShaderType");
}

VkShaderStageFlags VulkanShader::get_vulkan_shader_stage_bits(ShaderType stage)
{
    VkShaderStageFlags bits = 0;

    if (stage & ShaderType::Vertex)
        bits |= VK_SHADER_STAGE_VERTEX_BIT;

    if (stage & ShaderType::Fragment)
        bits |= VK_SHADER_STAGE_FRAGMENT_BIT;

    if (stage & ShaderType::Compute)
        bits |= VK_SHADER_STAGE_COMPUTE_BIT;

    if (stage & ShaderType::RtxRaygen)
        bits |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    if (stage & ShaderType::RtxAnyHit)
        bits |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    if (stage & ShaderType::RtxClosestHit)
        bits |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    if (stage & ShaderType::RtxMiss)
        bits |= VK_SHADER_STAGE_MISS_BIT_KHR;

    if (stage & ShaderType::RtxIntersection)
        bits |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

    MIZU_ASSERT(bits != 0, "ShaderType does not contain any valid shader type bits");

    return bits;
}

VkDescriptorType VulkanShader::get_vulkan_descriptor_type(const ShaderPropertyT& value)
{
    if (std::holds_alternative<ShaderImageProperty>(value))
    {
        const ShaderImageProperty& image_val = std::get<ShaderImageProperty>(value);

        switch (image_val.type)
        {
        case ShaderImageProperty::Type::Sampled:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case ShaderImageProperty::Type::Separate:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ShaderImageProperty::Type::Storage:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
    }
    else if (std::holds_alternative<ShaderBufferProperty>(value))
    {
        const ShaderBufferProperty& buffer_val = std::get<ShaderBufferProperty>(value);

        switch (buffer_val.type)
        {
        case ShaderBufferProperty::Type::Uniform:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ShaderBufferProperty::Type::Storage:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
    }
    else if (std::holds_alternative<ShaderSamplerProperty>(value))
    {
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    else if (std::holds_alternative<ShaderRtxAccelerationStructureProperty>(value))
    {
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }

    MIZU_UNREACHABLE("ShaderPropertyT should only have specified types in variant");
}

const std::vector<ShaderProperty>& VulkanShader::get_properties() const
{
    return m_reflection->get_properties();
}

const std::vector<ShaderConstant>& VulkanShader::get_constants() const
{
    return m_reflection->get_constants();
}

const std::vector<ShaderInput>& VulkanShader::get_inputs() const
{
    return m_reflection->get_inputs();
}

const std::vector<ShaderOutput>& VulkanShader::get_outputs() const
{
    return m_reflection->get_outputs();
}

} // namespace Mizu::Vulkan
