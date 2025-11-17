#include "vulkan_shader.h"

#include <ranges>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/io/filesystem.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_utils.h"
#include "render_core/shader/shader_reflection.h"

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

    const std::filesystem::path reflection_path = m_description.path.string() + ".json";
    MIZU_ASSERT(std::filesystem::exists(reflection_path), "Reflection path does not exist");

    const std::string reflection_content = Filesystem::read_file_string(reflection_path);
    m_reflection = std::make_unique<SlangReflection>(reflection_content);
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

VkDescriptorType VulkanShader::get_vulkan_descriptor_type(const ShaderResourceT& value)
{
    if (std::holds_alternative<ShaderResourceTexture>(value))
    {
        const ShaderResourceTexture& texture = std::get<ShaderResourceTexture>(value);

        switch (texture.access)
        {
        case ShaderResourceAccessType::ReadOnly:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ShaderResourceAccessType::ReadWrite:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
    }
    else if (std::holds_alternative<ShaderResourceTextureCube>(value))
    {
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
    else if (std::holds_alternative<ShaderResourceStructuredBuffer>(value))
    {
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    else if (std::holds_alternative<ShaderResourceByteAddressBuffer>(value))
    {
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    else if (std::holds_alternative<ShaderResourceConstantBuffer>(value))
    {
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    else if (std::holds_alternative<ShaderResourceSamplerState>(value))
    {
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    else if (std::holds_alternative<ShaderResourceAccelerationStructure>(value))
    {
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }

    MIZU_UNREACHABLE("ShaderResourceT should only have specified types in variant");

    return VK_DESCRIPTOR_TYPE_MAX_ENUM; // Default to prevent compilation errors
}

const SlangReflection& VulkanShader::get_reflection() const
{
    return *m_reflection;
}

} // namespace Mizu::Vulkan
