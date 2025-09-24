#include "vulkan_ray_tracing_pipeline.h"

#include <cstring>

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu::Vulkan
{

VulkanRayTracingPipeline::VulkanRayTracingPipeline(Description desc) : m_description(std::move(desc))
{
    //
    // Create pipeline
    //

    MIZU_ASSERT(m_description.raygen_shader != nullptr, "Raygen shader is required");

    m_raygen_shader = std::dynamic_pointer_cast<VulkanShader>(m_description.raygen_shader);

    m_miss_shaders.resize(m_description.miss_shaders.size());
    for (uint32_t i = 0; i < m_description.miss_shaders.size(); ++i)
    {
        m_miss_shaders[i] = std::dynamic_pointer_cast<VulkanShader>(m_description.miss_shaders[i]);
    }

    m_closest_hit_shaders.resize(m_description.closest_hit_shaders.size());
    for (uint32_t i = 0; i < m_description.closest_hit_shaders.size(); ++i)
    {
        m_closest_hit_shaders[i] = std::dynamic_pointer_cast<VulkanShader>(m_description.closest_hit_shaders[i]);
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(1 + m_miss_shaders.size() + m_closest_hit_shaders.size());

    stages.push_back(m_raygen_shader->get_stage_create_info());

    for (const std::shared_ptr<VulkanShader>& shader : m_miss_shaders)
    {
        stages.push_back(shader->get_stage_create_info());
    }

    for (const std::shared_ptr<VulkanShader>& shader : m_closest_hit_shaders)
    {
        stages.push_back(shader->get_stage_create_info());
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    groups.reserve(stages.size());

    VkRayTracingShaderGroupCreateInfoKHR shader_group_create_info_template{};
    shader_group_create_info_template.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shader_group_create_info_template.generalShader = VK_SHADER_UNUSED_KHR;
    shader_group_create_info_template.closestHitShader = VK_SHADER_UNUSED_KHR;
    shader_group_create_info_template.anyHitShader = VK_SHADER_UNUSED_KHR;
    shader_group_create_info_template.intersectionShader = VK_SHADER_UNUSED_KHR;

    uint32_t groups_idx = 0;

    {
        // raygen shader

        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(shader_group_create_info_template);

        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = groups_idx;

        groups_idx += 1;
    }

    // miss shaders

    for (uint32_t i = 0; i < m_miss_shaders.size(); ++i)
    {
        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(shader_group_create_info_template);

        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = groups_idx;

        groups_idx += 1;
    }

    // closest hit shaders

    for (uint32_t i = 0; i < m_closest_hit_shaders.size(); ++i)
    {
        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(shader_group_create_info_template);

        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.closestHitShader = groups_idx;

        groups_idx += 1;
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
        vkCreateRayTracingPipelinesKHR(VulkanContext.device->handle(), {}, {}, 1, &create_info, nullptr, &m_pipeline));

    //
    // Create SBT
    //

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtx_props = get_rtx_properties();

    const uint32_t miss_count = static_cast<uint32_t>(m_miss_shaders.size());
    const uint32_t hit_count = static_cast<uint32_t>(m_closest_hit_shaders.size());

    const uint32_t handle_count = miss_count + hit_count + 1;

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
        VulkanContext.device->handle(), m_pipeline, 0, handle_count, data_size, handles.data()));

    BufferDescription sbt_desc{};
    sbt_desc.size = m_ray_generation_region.size + m_miss_region.size + m_hit_region.size;
    sbt_desc.usage = BufferUsageBits::RtxShaderBindingTable | BufferUsageBits::HostVisible;
    sbt_desc.name = "SBT_buffer";

    m_sbt_buffer = std::make_unique<VulkanBufferResource>(sbt_desc);

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
}

VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(VulkanContext.device->handle(), m_pipeline_layout, nullptr);
}

void VulkanRayTracingPipeline::create_pipeline_layout()
{
    // Gather resources

    m_shader_group = ShaderGroup();
    m_shader_group.add_shader(*m_raygen_shader);
    for (const auto& shader : m_miss_shaders)
        m_shader_group.add_shader(*shader);
    for (const auto& shader : m_closest_hit_shaders)
        m_shader_group.add_shader(*shader);

    // Create pipeline layout

    m_set_layouts.clear();
    std::vector<VkPushConstantRange> push_constant_ranges;

    for (uint32_t set = 0; set < m_shader_group.get_max_set(); ++set)
    {
        const std::vector<ShaderProperty>& properties = m_shader_group.get_properties_in_set(set);

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        layout_bindings.reserve(properties.size());

        for (const ShaderProperty& property : properties)
        {
            VkDescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = property.binding_info.binding;
            layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(property.value);
            layout_binding.descriptorCount = 1;
            layout_binding.stageFlags =
                VulkanShader::get_vulkan_shader_stage_bits(m_shader_group.get_resource_stage_bits(property.name));
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

    for (const ShaderConstant& constant : m_shader_group.get_constants())
    {
        VkPushConstantRange range{};
        range.stageFlags =
            VulkanShader::get_vulkan_shader_stage_bits(m_shader_group.get_resource_stage_bits(constant.name));
        range.offset = 0;
        range.size = constant.size;

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