#include "vulkan_ray_tracing_pipeline.h"

#include <array>

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_ray_tracing_shader.h"

namespace Mizu::Vulkan
{

VulkanRayTracingPipeline::VulkanRayTracingPipeline(Description desc) : m_description(std::move(desc))
{
    /*

    //
    // Create pipeline
    //

    const VulkanRayTracingShader& native_ray_generation_shader =
        dynamic_cast<const VulkanRayTracingShader&>(*m_description.ray_generation_shader);
    const VulkanRayTracingShader& native_miss_shader =
        dynamic_cast<const VulkanRayTracingShader&>(*m_description.miss_shader);
    const VulkanRayTracingShader& native_closest_hit_shader =
        dynamic_cast<const VulkanRayTracingShader&>(*m_description.closest_hit_shader);

    std::array<VkPipelineShaderStageCreateInfo, 3> stages{};

    stages[0] = native_ray_generation_shader.get_stage_create_info();
    stages[1] = native_miss_shader.get_stage_create_info();
    stages[2] = native_closest_hit_shader.get_stage_create_info();

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{};

    VkRayTracingShaderGroupCreateInfoKHR shader_group_create_info{};
    shader_group_create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shader_group_create_info.anyHitShader = VK_SHADER_UNUSED_KHR;
    shader_group_create_info.closestHitShader = VK_SHADER_UNUSED_KHR;
    shader_group_create_info.generalShader = VK_SHADER_UNUSED_KHR;
    shader_group_create_info.intersectionShader = VK_SHADER_UNUSED_KHR;

    {
        // raygen shader

        groups[0] = shader_group_create_info;

        groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groups[0].generalShader = 0;
    }

    {
        // miss shader

        groups[1] = shader_group_create_info;

        groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groups[1].generalShader = 1;
    }

    {
        // closest hit shader

        groups[2] = shader_group_create_info;

        groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        groups[2].closestHitShader = 2;
    }

    create_pipeline_layout();

    VkRayTracingPipelineCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    create_info.stageCount = static_cast<uint32_t>(stages.size());
    create_info.pStages = stages.data();
    create_info.groupCount = static_cast<uint32_t>(groups.size());
    create_info.pGroups = groups.data();
    create_info.maxPipelineRayRecursionDepth = m_description.max_ray_recursion_depth;
    create_info.layout = m_pipeline_layout;

    VK_CHECK(
        vkCreateRayTracingPipelinesKHR(VulkanContext.device->handle(), {}, {}, 1, &create_info, nullptr, &m_handle));

    //
    // Create SBT
    //

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtx_props = get_rtx_properties();

    constexpr uint32_t miss_count = 1;
    constexpr uint32_t hit_count = 1;

    constexpr uint32_t handle_count = miss_count + hit_count + 1;

    const uint32_t handle_size = rtx_props.shaderGroupHandleSize;
    const uint32_t handle_alignment = rtx_props.shaderGroupHandleAlignment;

    const auto align_up = [](uint32_t size, uint32_t alignment) -> uint32_t {
        return (size + alignment - 1) & ~(alignment - 1);
    };

    const uint32_t handle_size_aligned = align_up(handle_size, handle_alignment);

    m_ray_generation_region.stride = align_up(handle_size_aligned, rtx_props.shaderGroupBaseAlignment);
    m_ray_generation_region.size = m_ray_generation_region.stride;

    m_miss_region.stride = handle_size_aligned;
    m_miss_region.size = align_up(miss_count * handle_size_aligned, rtx_props.shaderGroupBaseAlignment);

    m_hit_region.stride = handle_size_aligned;
    m_hit_region.size = align_up(hit_count * handle_size_aligned, rtx_props.shaderGroupBaseAlignment);

    const uint32_t data_size = handle_count * handle_size;

    std::vector<uint8_t> handles(data_size);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
        VulkanContext.device->handle(), m_handle, 0, handle_count, data_size, handles.data()));

    BufferDescription sbt_desc{};
    sbt_desc.size = m_ray_generation_region.size + m_miss_region.size + m_hit_region.size;
    sbt_desc.usage =
        BufferUsageBits::TransferSrc | BufferUsageBits::RtxShaderBindingTable | BufferUsageBits::HostVisible;
    sbt_desc.name = "SBT_buffer";

    // TODO: Don't use Renderer::get_allocator()
    m_sbt_buffer = std::make_unique<VulkanBufferResource>(sbt_desc, Renderer::get_allocator());

    VkBufferDeviceAddressInfo device_address_info{};
    device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    device_address_info.buffer = m_sbt_buffer->handle();

    const VkDeviceAddress sbt_address = vkGetBufferDeviceAddress(VulkanContext.device->handle(), &device_address_info);

    m_ray_generation_region.deviceAddress = sbt_address;
    m_miss_region.deviceAddress = sbt_address + m_ray_generation_region.size;
    m_hit_region.deviceAddress = sbt_address + m_ray_generation_region.size + m_miss_region.size;

    const auto get_handle = [&](uint32_t i) {
        return handles.data() + i * handle_size;
    };

    std::vector<uint8_t> sbt_data(sbt_desc.size);
    uint32_t handle_idx = 0;

    // ray generation
    memcpy(sbt_data.data(), get_handle(handle_idx), handle_size);
    handle_idx += 1;

    // miss
    uint8_t* miss_dst = sbt_data.data() + m_ray_generation_region.size;
    for (uint32_t i = 0; i < miss_count; ++i)
    {
        memcpy(miss_dst, get_handle(handle_idx), handle_size);
        handle_idx += 1;

        miss_dst += m_miss_region.stride;
    }

    // hit
    uint8_t* hit_dst = sbt_data.data() + m_ray_generation_region.size + m_miss_region.size;
    for (uint32_t i = 0; i < hit_count; ++i)
    {
        memcpy(hit_dst, get_handle(handle_idx), handle_size);
        handle_idx += 1;

        hit_dst += m_hit_region.stride;
    }

    m_sbt_buffer->set_data(sbt_data.data());

    */
}

VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_handle, nullptr);
    vkDestroyPipelineLayout(VulkanContext.device->handle(), m_pipeline_layout, nullptr);
}

std::shared_ptr<VulkanRayTracingShader> VulkanRayTracingPipeline::get_ray_generation_shader() const
{
    return std::dynamic_pointer_cast<VulkanRayTracingShader>(m_description.ray_generation_shader);
}

void VulkanRayTracingPipeline::create_pipeline_layout()
{
    /*

    std::vector<ShaderProperty> properties;
    std::vector<ShaderConstant> constants;
    std::unordered_map<std::string, VkShaderStageFlags> uniform_to_stage;

    const auto retrieve_properties_constants = [&](const std::shared_ptr<VulkanShaderBase>& shader) {
        for (const ShaderProperty& property : shader->get_properties())
        {
            properties.push_back(property);

            const VkShaderStageFlags stage = *shader->get_property_stage(property.name);

            auto [it, inserted] = uniform_to_stage.try_emplace(property.name, stage);
            if (!inserted)
            {
                it->second |= stage;
            }
        }

        for (const ShaderConstant& constant : shader->get_constants())
        {
            constants.push_back(constant);

            const VkShaderStageFlags stage = *shader->get_property_stage(constant.name);

            auto [it, inserted] = uniform_to_stage.try_emplace(constant.name, stage);
            if (!inserted)
            {
                it->second |= stage;
            }
        }
    };

    retrieve_properties_constants(
        std::dynamic_pointer_cast<VulkanRayTracingShader>(m_description.ray_generation_shader));
    retrieve_properties_constants(std::dynamic_pointer_cast<VulkanRayTracingShader>(m_description.miss_shader));
    retrieve_properties_constants(std::dynamic_pointer_cast<VulkanRayTracingShader>(m_description.closest_hit_shader));

    std::vector<std::vector<ShaderProperty>> set_properties;

    for (const ShaderProperty& property : properties)
    {
        if (property.binding_info.set >= set_properties.size())
        {
            for (size_t i = set_properties.size(); i < property.binding_info.set + 1; ++i)
            {
                set_properties.emplace_back();
            }
        }

        set_properties[property.binding_info.set].push_back(property);
    }

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts(set_properties.size());

    for (size_t i = 0; i < set_properties.size(); ++i)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& property : set_properties[i])
        {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = property.binding_info.binding;
            binding.descriptorType = VulkanShaderBase::get_vulkan_descriptor_type(property.value);
            binding.descriptorCount = 1;
            binding.stageFlags = uniform_to_stage[property.name];
            binding.pImmutableSamplers = nullptr;

            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.bindingCount = static_cast<uint32_t>(bindings.size());
        create_info.pBindings = bindings.data();

        descriptor_set_layouts[i] = VulkanContext.layout_cache->create_descriptor_layout(create_info);
    }

    std::vector<VkPushConstantRange> push_constant_ranges;

    for (const ShaderConstant& constant : constants)
    {
        VkPushConstantRange constant_range{};
        constant_range.stageFlags = uniform_to_stage[constant.name];
        constant_range.offset = 0;
        constant_range.size = constant.size;

        push_constant_ranges.push_back(constant_range);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

    VK_CHECK(vkCreatePipelineLayout(
        VulkanContext.device->handle(), &pipeline_layout_create_info, nullptr, &m_pipeline_layout));

    */
}

} // namespace Mizu::Vulkan