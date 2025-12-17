#include "backend/vulkan/vulkan_shader.h"

#include <ranges>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/io/filesystem.h"

#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_core.h"
#include "backend/vulkan/vulkan_descriptors.h"
#include "backend/vulkan/vulkan_utils.h"

namespace Mizu::Vulkan
{

VulkanShader::VulkanShader(ShaderDescription desc) : m_description(std::move(desc))
{
    const auto source = Filesystem::read_file(m_description.path);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = source.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(source.data());

    VK_CHECK(vkCreateShaderModule(VulkanContext.device->handle(), &create_info, nullptr, &m_handle));
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

VkDescriptorType VulkanShader::get_vulkan_descriptor_type(ShaderResourceType type)
{
    switch (type)
    {
    case ShaderResourceType::TextureSrv:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    case ShaderResourceType::TextureUav:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    case ShaderResourceType::StructuredBufferSrv:
    case ShaderResourceType::StructuredBufferUav:
    case ShaderResourceType::ByteAddressBufferSrv:
    case ShaderResourceType::ByteAddressBufferUav:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    case ShaderResourceType::ConstantBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    case ShaderResourceType::SamplerState:
        return VK_DESCRIPTOR_TYPE_SAMPLER;

    case ShaderResourceType::AccelerationStructure:
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    case ShaderResourceType::PushConstant:
        MIZU_UNREACHABLE("Invalid resource type");
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

} // namespace Mizu::Vulkan
