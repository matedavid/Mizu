#include "vulkan_pipeline.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu::Vulkan
{

// Rasterization helpers

static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode)
{
    using PolygonMode = RasterizationState::PolygonMode;

    switch (mode)
    {
    case PolygonMode::Fill:
        return VK_POLYGON_MODE_FILL;
    case PolygonMode::Line:
        return VK_POLYGON_MODE_LINE;
    case PolygonMode::Point:
        return VK_POLYGON_MODE_POINT;
    }
}

static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode)
{
    using CullMode = RasterizationState::CullMode;

    switch (mode)
    {
    case CullMode::None:
        return VK_CULL_MODE_NONE;
    case CullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    case CullMode::FrontAndBack:
        return VK_CULL_MODE_FRONT_AND_BACK;
    }
}

static VkFrontFace get_front_face(RasterizationState::FrontFace mode)
{
    using FrontFace = RasterizationState::FrontFace;

    switch (mode)
    {
    case FrontFace::CounterClockwise:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case FrontFace::ClockWise:
        return VK_FRONT_FACE_CLOCKWISE;
    }
}

// Depth Stencil helpers

static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op)
{
    using DepthCompareOp = DepthStencilState::DepthCompareOp;

    switch (op)
    {
    case DepthCompareOp::Never:
        return VK_COMPARE_OP_NEVER;
    case DepthCompareOp::Less:
        return VK_COMPARE_OP_LESS;
    case DepthCompareOp::Equal:
        return VK_COMPARE_OP_EQUAL;
    case DepthCompareOp::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case DepthCompareOp::Greater:
        return VK_COMPARE_OP_GREATER;
    case DepthCompareOp::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case DepthCompareOp::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case DepthCompareOp::Always:
        return VK_COMPARE_OP_ALWAYS;
    }
}

// Color Blend helpers

static VkBlendFactor get_blend_factor(ColorBlendState::BlendFactor factor)
{
    using BlendFactor = ColorBlendState::BlendFactor;

    switch (factor)
    {
    case BlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SourceAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }

    MIZU_UNREACHABLE("Unimplemented or Invalid BlendFactor");

    return VK_BLEND_FACTOR_ZERO; // Default return to prevent compilation
}

static VkBlendOp get_blend_operation(ColorBlendState::BlendOperation operation)
{
    using BlendOperation = ColorBlendState::BlendOperation;

    switch (operation)
    {
    case BlendOperation::Add:
        return VK_BLEND_OP_ADD;
    case BlendOperation::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case BlendOperation::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendOperation::Min:
        return VK_BLEND_OP_MIN;
    case BlendOperation::Max:
        return VK_BLEND_OP_MAX;
    }

    MIZU_UNREACHABLE("Unimplemented or Invalid BlendOperation");

    return VK_BLEND_OP_ADD; // Default return to prevent compilation errors
}

static VkColorComponentFlags get_color_component_flags(ColorBlendState::ColorComponentBits bits)
{
    using ColorComponentBits = ColorBlendState::ColorComponentBits;

    VkColorComponentFlags flags = 0;
    if (bits & ColorComponentBits::Red)
        flags |= VK_COLOR_COMPONENT_R_BIT;
    if (bits & ColorComponentBits::Green)
        flags |= VK_COLOR_COMPONENT_G_BIT;
    if (bits & ColorComponentBits::Blue)
        flags |= VK_COLOR_COMPONENT_B_BIT;
    if (bits & ColorComponentBits::Alpha)
        flags |= VK_COLOR_COMPONENT_A_BIT;

    return flags;
}

static VkLogicOp get_logic_operation(ColorBlendState::LogicOperation operation)
{
    using LogicOperation = ColorBlendState::LogicOperation;

    switch (operation)
    {
    case LogicOperation::Clear:
        return VK_LOGIC_OP_CLEAR;
    }

    MIZU_UNREACHABLE("Unimplemented or Invalid LogicOperation");

    return VK_LOGIC_OP_CLEAR; // Default return to prevent compilation errors
}

// Other helpers

static void create_pipeline_layout(
    const ShaderGroup& shader_group,
    VkPipelineLayout& pipeline_layout,
    std::vector<VkDescriptorSetLayout>& set_layouts)
{
    constexpr size_t MAX_PUSH_CONSTANT_RANGES = 10;
    inplace_vector<VkPushConstantRange, MAX_PUSH_CONSTANT_RANGES> push_constant_ranges;

    for (uint32_t set = 0; set < shader_group.get_max_set(); ++set)
    {
        const std::vector<ShaderResource>& parameters = shader_group.get_parameters_in_set(set);

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        layout_bindings.reserve(parameters.size());

        for (const ShaderResource& parameter : parameters)
        {
            VkDescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = parameter.binding_info.binding;
            layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(parameter.value);
            layout_binding.descriptorCount = 1;
            layout_binding.stageFlags =
                VulkanShader::get_vulkan_shader_stage_bits(shader_group.get_resource_stage_bits(parameter.name));
            layout_binding.pImmutableSamplers = nullptr;

            layout_bindings.push_back(layout_binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_create_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
        layout_create_info.pBindings = layout_bindings.data();

        VkDescriptorSetLayout layout = VulkanContext.layout_cache->create_descriptor_layout(layout_create_info);
        set_layouts.push_back(layout);
    }

    for (const ShaderPushConstant& constant : shader_group.get_constants())
    {
        VkPushConstantRange range{};
        range.stageFlags =
            VulkanShader::get_vulkan_shader_stage_bits(shader_group.get_resource_stage_bits(constant.name));
        range.offset = 0;
        range.size = static_cast<uint32_t>(constant.size);

        push_constant_ranges.push_back(range);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

    VK_CHECK(vkCreatePipelineLayout(
        VulkanContext.device->handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
}

//
// GraphicsPipeline
//

VulkanPipeline::VulkanPipeline(const GraphicsPipelineDescription& desc) : m_pipeline_type(PipelineType::Graphics)
{
    MIZU_ASSERT(
        desc.vertex_shader != nullptr && desc.vertex_shader->get_type() == ShaderType::Vertex,
        "No vertex shader provided in GraphicsPipeline");
    MIZU_ASSERT(
        desc.fragment_shader != nullptr && desc.fragment_shader->get_type() == ShaderType::Fragment,
        "No fragment shader provided in GraphicsPipeline");

    MIZU_ASSERT(desc.target_framebuffer != nullptr, "Target framebuffer not provided in GraphicsPipeline");

    // Shaders
    const VulkanShader& native_vertex_shader = static_cast<const VulkanShader&>(*desc.vertex_shader);
    const VulkanShader& native_fragment_shader = static_cast<const VulkanShader&>(*desc.fragment_shader);

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
        native_vertex_shader.get_stage_create_info(),
        native_fragment_shader.get_stage_create_info(),
    };

    m_shader_group = ShaderGroup{};
    m_shader_group.add_shader(native_vertex_shader);
    m_shader_group.add_shader(native_fragment_shader);

    // Pipeline layout
    create_pipeline_layout(m_shader_group, m_pipeline_layout, m_set_layouts);

    // Vertex input
    VkVertexInputBindingDescription binding_description{};

    binding_description = VkVertexInputBindingDescription{};
    binding_description.binding = 0;
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    const auto shader_primitive_type_to_vk_format = [](ShaderPrimitiveType type) -> VkFormat {
        switch (type)
        {
        case ShaderPrimitiveType::Float:
            return VK_FORMAT_R32_SFLOAT;
        case ShaderPrimitiveType::Float2:
            return VK_FORMAT_R32G32_SFLOAT;
        case ShaderPrimitiveType::Float3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case ShaderPrimitiveType::Float4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            MIZU_UNREACHABLE("Not implemented shader primitive type");
            return VK_FORMAT_UNDEFINED; // Default return value to prevent compilation error
        }
    };

    constexpr size_t MAX_VERTEX_INPUT_ATTRIBUTE_DESCRIPTIONS = 20;
    inplace_vector<VkVertexInputAttributeDescription, MAX_VERTEX_INPUT_ATTRIBUTE_DESCRIPTIONS> attribute_descriptions{};
    MIZU_ASSERT(
        native_vertex_shader.get_reflection().get_inputs().size() < MAX_VERTEX_INPUT_ATTRIBUTE_DESCRIPTIONS,
        "Number of vertex inputs is greater than the maximum allowed vertex input attribute descriptions");

    uint32_t stride = 0;
    for (const ShaderInputOutput& input_var : native_vertex_shader.get_reflection().get_inputs())
    {
        VkVertexInputAttributeDescription description{};
        description.binding = 0;
        description.location = input_var.location;
        description.format = shader_primitive_type_to_vk_format(input_var.primitive.type);
        description.offset = stride;

        attribute_descriptions.push_back(description);

        stride += ShaderPrimitiveType::size(input_var.primitive.type);
    }

    binding_description.stride = stride;

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_description;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input.pVertexAttributeDescriptions = attribute_descriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // TODO: Make this configurable?
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Tessellation (not using)
    VkPipelineTessellationStateCreateInfo tessellation{};
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

    // Viewport (using dynamic viewport and scissor)
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    // Rasterization
    bool depth_clamp_enable = desc.rasterization.depth_clamp;
    if (depth_clamp_enable && !Renderer::get_capabilities().depth_clamp_enabled)
    {
        MIZU_LOG_ONCE_ERROR(
            "Requesting DepthClamp enabled but feature is not supported by Physical Device, setting to false");
        depth_clamp_enable = false;
    }

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = depth_clamp_enable;
    rasterization.rasterizerDiscardEnable = desc.rasterization.rasterizer_discard;
    rasterization.polygonMode = get_polygon_mode(desc.rasterization.polygon_mode);
    rasterization.cullMode = get_cull_mode(desc.rasterization.cull_mode);
    rasterization.frontFace = get_front_face(desc.rasterization.front_face);
    rasterization.depthBiasEnable = desc.rasterization.depth_bias.enabled;
    rasterization.depthBiasConstantFactor = desc.rasterization.depth_bias.constant_factor;
    rasterization.depthBiasClamp = desc.rasterization.depth_bias.clamp;
    rasterization.depthBiasSlopeFactor = desc.rasterization.depth_bias.slope_factor;
    rasterization.lineWidth = 1.0f;

    // Multisample
    // TODO: Make configurable
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth Stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = desc.depth_stencil.depth_test;
    depth_stencil.depthWriteEnable = desc.depth_stencil.depth_write;
    depth_stencil.depthCompareOp = get_depth_compare_op(desc.depth_stencil.depth_compare_op);
    depth_stencil.depthBoundsTestEnable = desc.depth_stencil.depth_bounds_test;
    depth_stencil.stencilTestEnable = desc.depth_stencil.stencil_test;
    // TODO: depth_stencil.front
    // TODO: depth_stencil.back
    depth_stencil.minDepthBounds = desc.depth_stencil.min_depth_bounds;
    depth_stencil.maxDepthBounds = desc.depth_stencil.max_depth_bounds;

    // Color blend
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;

    const std::span<const FramebufferAttachment> color_attachments = desc.target_framebuffer->get_color_attachments();
    for (uint32_t i = 0; i < color_attachments.size(); ++i)
    {
        if (desc.color_blend.method == ColorBlendState::Method::None)
        {
            VkPipelineColorBlendAttachmentState state{};
            state.blendEnable = VK_FALSE;
            state.colorWriteMask = get_color_component_flags(ColorBlendState::ColorComponentBits::All);

            color_blend_attachments.push_back(state);

            continue;
        }

        MIZU_ASSERT(
            i < desc.color_blend.attachments.size(),
            "Attachment with idx {} does not have a corresponding attachments value");
        const ColorBlendState::AttachmentState& attachment_state = desc.color_blend.attachments[i];

        VkPipelineColorBlendAttachmentState state{};
        state.blendEnable = attachment_state.blend_enabled;
        state.srcColorBlendFactor = get_blend_factor(attachment_state.src_color_blend_factor);
        state.dstColorBlendFactor = get_blend_factor(attachment_state.dst_color_blend_factor);
        state.colorBlendOp = get_blend_operation(attachment_state.color_blend_op);
        state.srcAlphaBlendFactor = get_blend_factor(attachment_state.src_alpha_blend_factor);
        state.dstAlphaBlendFactor = get_blend_factor(attachment_state.dst_alpha_blend_factor);
        state.alphaBlendOp = get_blend_operation(attachment_state.alpha_blend_op);
        state.colorWriteMask = get_color_component_flags(attachment_state.color_write_mask);

        color_blend_attachments.push_back(state);
    }

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.logicOpEnable = desc.color_blend.method == ColorBlendState::Method::LogicOperations;
    color_blend.logicOp = get_logic_operation(desc.color_blend.logic_op);
    color_blend.attachmentCount = static_cast<uint32_t>(color_blend_attachments.size());
    color_blend.pAttachments = color_blend_attachments.data();
    color_blend.blendConstants[0] = desc.color_blend.blend_constants.r;
    color_blend.blendConstants[1] = desc.color_blend.blend_constants.g;
    color_blend.blendConstants[2] = desc.color_blend.blend_constants.b;
    color_blend.blendConstants[3] = desc.color_blend.blend_constants.a;

    // Dynamic state
    constexpr std::array<VkDynamicState, 2> dynamic_state_vals = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_state_vals.size());
    dynamic_state.pDynamicStates = dynamic_state_vals.data();

    //
    // Create Pipeline
    //

    const VulkanFramebuffer& native_framebuffer = static_cast<const VulkanFramebuffer&>(*desc.target_framebuffer);

    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    create_info.pStages = shader_stages.data();
    create_info.pVertexInputState = &vertex_input;
    create_info.pInputAssemblyState = &input_assembly;
    create_info.pTessellationState = &tessellation;
    create_info.pViewportState = &viewport;
    create_info.pRasterizationState = &rasterization;
    create_info.pMultisampleState = &multisample;
    create_info.pDepthStencilState = &depth_stencil;
    create_info.pColorBlendState = &color_blend;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = m_pipeline_layout;
    create_info.renderPass = native_framebuffer.get_render_pass();
    create_info.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(VulkanContext.device->handle(), nullptr, 1, &create_info, nullptr, &m_pipeline));
}

//
// ComputePipeline
//

VulkanPipeline::VulkanPipeline(const ComputePipelineDescription& desc) : m_pipeline_type(PipelineType::Compute)
{
    MIZU_ASSERT(
        desc.compute_shader != nullptr && desc.compute_shader->get_type() == ShaderType::Compute,
        "No compute shader provided in ComputePipeline");

    const VulkanShader& native_compute_shader = static_cast<const VulkanShader&>(*desc.compute_shader);

    m_shader_group = ShaderGroup{};
    m_shader_group.add_shader(native_compute_shader);

    create_pipeline_layout(m_shader_group, m_pipeline_layout, m_set_layouts);

    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.stage = native_compute_shader.get_stage_create_info();
    create_info.layout = m_pipeline_layout;

    VK_CHECK(vkCreateComputePipelines(
        VulkanContext.device->handle(), VK_NULL_HANDLE, 1, &create_info, nullptr, &m_pipeline));
}

//
// RayTracingPipeline
//

VulkanPipeline::VulkanPipeline(const RayTracingPipelineDescription& desc) : m_pipeline_type(PipelineType::RayTracing)
{
    MIZU_ASSERT(
        Renderer::get_capabilities().ray_tracing_hardware,
        "Can't create RayTracingPipeline because ray tracing hardware is not supported");
    MIZU_ASSERT(desc.raygen_shader != nullptr, "Raygen shader is required in RayTracingPipeline");

    const VulkanShader& native_raygen_shader = static_cast<const VulkanShader&>(*desc.raygen_shader);

    constexpr size_t MAX_VARIABLE_NUM_SHADERS = RayTracingPipelineDescription::MAX_VARIABLE_NUM_SHADERS;

    inplace_vector<std::shared_ptr<VulkanShader>, MAX_VARIABLE_NUM_SHADERS> miss_shaders;
    for (const auto& shader : desc.miss_shaders)
    {
        miss_shaders.push_back(std::static_pointer_cast<VulkanShader>(shader));
    }

    inplace_vector<std::shared_ptr<VulkanShader>, MAX_VARIABLE_NUM_SHADERS> closest_hit_shaders;
    for (const auto& shader : desc.closest_hit_shaders)
    {
        closest_hit_shaders.push_back(std::static_pointer_cast<VulkanShader>(shader));
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(1 + miss_shaders.size() + closest_hit_shaders.size());

    stages.push_back(native_raygen_shader.get_stage_create_info());

    for (const std::shared_ptr<VulkanShader>& shader : miss_shaders)
    {
        stages.push_back(shader->get_stage_create_info());
    }

    for (const std::shared_ptr<VulkanShader>& shader : closest_hit_shaders)
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

    for (uint32_t i = 0; i < miss_shaders.size(); ++i)
    {
        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(shader_group_create_info_template);

        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = groups_idx;

        groups_idx += 1;
    }

    // closest hit shaders

    for (uint32_t i = 0; i < closest_hit_shaders.size(); ++i)
    {
        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(shader_group_create_info_template);

        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.closestHitShader = groups_idx;

        groups_idx += 1;
    }

    m_shader_group = ShaderGroup{};
    m_shader_group.add_shader(native_raygen_shader);
    for (const auto& shader : miss_shaders)
        m_shader_group.add_shader(*shader);
    for (const auto& shader : closest_hit_shaders)
        m_shader_group.add_shader(*shader);

    create_pipeline_layout(m_shader_group, m_pipeline_layout, m_set_layouts);

    //
    // Create pipeline
    //

    VkRayTracingPipelineCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    create_info.stageCount = static_cast<uint32_t>(stages.size());
    create_info.pStages = stages.data();
    create_info.groupCount = static_cast<uint32_t>(groups.size());
    create_info.pGroups = groups.data();
    create_info.maxPipelineRayRecursionDepth = desc.max_ray_recursion_depth;
    create_info.layout = m_pipeline_layout;

    VK_CHECK(
        vkCreateRayTracingPipelinesKHR(VulkanContext.device->handle(), {}, {}, 1, &create_info, nullptr, &m_pipeline));

    //
    // Create SBT
    //

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtx_props = VulkanContext.rtx_properties;

    const uint32_t miss_count = static_cast<uint32_t>(miss_shaders.size());
    const uint32_t hit_count = static_cast<uint32_t>(closest_hit_shaders.size());

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

//
// Other
//

VulkanPipeline::~VulkanPipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(VulkanContext.device->handle(), m_pipeline_layout, nullptr);
}

VkPipelineBindPoint VulkanPipeline::get_vulkan_pipeline_bind_point(PipelineType type)
{
    switch (type)
    {
    case PipelineType::Graphics:
        return VK_PIPELINE_BIND_POINT_GRAPHICS;
    case PipelineType::Compute:
        return VK_PIPELINE_BIND_POINT_COMPUTE;
    case PipelineType::RayTracing:
        return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
    }
}

static inline const VkStridedDeviceAddressRegionKHR& get_rtx_shader_region(
    [[maybe_unused]] PipelineType pipeline_type,
    const VkStridedDeviceAddressRegionKHR& region)
{
    MIZU_ASSERT(
        pipeline_type == PipelineType::RayTracing, "Can't get rtx shader region if pipeline is not RayTracingPipeline");
    return region;
}

const VkStridedDeviceAddressRegionKHR& VulkanPipeline::get_ray_generation_region() const
{
    return get_rtx_shader_region(m_pipeline_type, m_ray_generation_region);
}

const VkStridedDeviceAddressRegionKHR& VulkanPipeline::get_miss_region() const
{
    return get_rtx_shader_region(m_pipeline_type, m_miss_region);
}

const VkStridedDeviceAddressRegionKHR& VulkanPipeline::get_hit_region() const
{
    return get_rtx_shader_region(m_pipeline_type, m_hit_region);
}

const VkStridedDeviceAddressRegionKHR& VulkanPipeline::get_call_region() const
{
    return get_rtx_shader_region(m_pipeline_type, m_call_region);
}

} // namespace Mizu::Vulkan