#include "vulkan_pipeline.h"

#include <cstring>
#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_shader.h"
#include "vulkan_types.h"

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

    // Shaders
    const VulkanShader& native_vertex_shader = static_cast<const VulkanShader&>(*desc.vertex_shader);
    const VulkanShader& native_fragment_shader = static_cast<const VulkanShader&>(*desc.fragment_shader);

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
        native_vertex_shader.get_stage_create_info(),
        native_fragment_shader.get_stage_create_info(),
    };

    // Pipeline layout
    m_pipeline_layout = VulkanContext.pipeline_layout_cache->get(desc.layout);
    get_push_constant_info_if_exists(desc.layout);

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
        desc.vertex_inputs.size() < MAX_VERTEX_INPUT_ATTRIBUTE_DESCRIPTIONS,
        "Number of vertex inputs is greater than the maximum allowed vertex input attribute descriptions");

    uint32_t stride = 0;
    for (const ShaderInputOutput& input_var : desc.vertex_inputs)
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
    if (depth_clamp_enable && !VulkanContext.device->get_properties().depth_clamp_enabled)
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

    const size_t num_color_attachments = desc.framebuffer_info.color_attachments.size();
    for (size_t i = 0; i < num_color_attachments; ++i)
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

    // Framebuffer

    inplace_vector<VkFormat, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachment_formats;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;

    for (const ImageFormat format : desc.framebuffer_info.color_attachments)
    {
        MIZU_ASSERT(!is_depth_format(format), "Color attachment can't have depth format");
        color_attachment_formats.push_back(get_vulkan_image_format(format));
    }

    if (desc.framebuffer_info.depth_stencil_attachment.has_value())
    {
        const ImageFormat format = *desc.framebuffer_info.depth_stencil_attachment;
        MIZU_ASSERT(is_depth_format(format), "Depth stencil attachment must have depth format");
        depth_format = get_vulkan_image_format(format);
    }

    VkPipelineRenderingCreateInfo rendering_create_info{};
    rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_create_info.colorAttachmentCount = static_cast<uint32_t>(color_attachment_formats.size());
    rendering_create_info.pColorAttachmentFormats = color_attachment_formats.data();
    rendering_create_info.depthAttachmentFormat = depth_format;
    rendering_create_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    //
    // Create Pipeline
    //

    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.pNext = &rendering_create_info;
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

    m_pipeline_layout = VulkanContext.pipeline_layout_cache->get(desc.layout);
    get_push_constant_info_if_exists(desc.layout);

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
        VulkanContext.device->get_properties().ray_tracing_hardware,
        "Can't create RayTracingPipeline because ray tracing hardware is not supported");

    MIZU_ASSERT(desc.raygen_shader != nullptr, "Raygen shader is required in RayTracingPipeline");

    const VulkanShader& native_raygen_shader = static_cast<const VulkanShader&>(*desc.raygen_shader);

    constexpr size_t MAX_VARIABLE_NUM_SHADERS = RayTracingPipelineDescription::MAX_VARIABLE_NUM_SHADERS;

    inplace_vector<std::shared_ptr<VulkanShader>, MAX_VARIABLE_NUM_SHADERS> miss_shaders;

    for (const auto& shader : desc.miss_shaders)
    {
        MIZU_ASSERT(shader != nullptr, "Null miss shader in RayTracingPipeline");
        miss_shaders.push_back(std::static_pointer_cast<VulkanShader>(shader));
    }

    struct VulkanHitGroup
    {
        std::shared_ptr<VulkanShader> closest_hit;
        std::shared_ptr<VulkanShader> any_hit;
        std::shared_ptr<VulkanShader> intersection;
    };

    inplace_vector<VulkanHitGroup, MAX_VARIABLE_NUM_SHADERS> hit_groups;

    for (const auto& src : desc.hit_groups)
    {
        MIZU_ASSERT(
            src.closest_hit_shader != nullptr || src.any_hit_shader != nullptr || src.intersection_shader != nullptr,
            "RayTracing hit group must contain at least one shader");

        VulkanHitGroup& dst = hit_groups.emplace_back();

        if (src.closest_hit_shader)
        {
            dst.closest_hit = std::static_pointer_cast<VulkanShader>(src.closest_hit_shader);
        }

        if (src.any_hit_shader)
        {
            dst.any_hit = std::static_pointer_cast<VulkanShader>(src.any_hit_shader);
        }

        if (src.intersection_shader)
        {
            dst.intersection = std::static_pointer_cast<VulkanShader>(src.intersection_shader);
        }
    }

    //
    // Shader stages
    //

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(1 + miss_shaders.size() + hit_groups.size() * 3);

    // Raygen stage

    stages.push_back(native_raygen_shader.get_stage_create_info());

    // Miss stages

    std::vector<uint32_t> miss_stage_indices;
    miss_stage_indices.reserve(miss_shaders.size());

    for (const auto& shader : miss_shaders)
    {
        miss_stage_indices.push_back(static_cast<uint32_t>(stages.size()));
        stages.push_back(shader->get_stage_create_info());
    }

    // Hit-group stages

    struct HitGroupStageIndices
    {
        uint32_t closest_hit = VK_SHADER_UNUSED_KHR;
        uint32_t any_hit = VK_SHADER_UNUSED_KHR;
        uint32_t intersection = VK_SHADER_UNUSED_KHR;
    };

    std::vector<HitGroupStageIndices> hit_group_stage_indices;

    hit_group_stage_indices.resize(hit_groups.size());

    for (uint32_t i = 0; i < hit_groups.size(); ++i)
    {
        HitGroupStageIndices& indices = hit_group_stage_indices[i];

        const VulkanHitGroup& group = hit_groups[i];

        if (group.closest_hit)
        {
            indices.closest_hit = static_cast<uint32_t>(stages.size());
            stages.push_back(group.closest_hit->get_stage_create_info());
        }

        if (group.any_hit)
        {
            indices.any_hit = static_cast<uint32_t>(stages.size());
            stages.push_back(group.any_hit->get_stage_create_info());
        }

        if (group.intersection)
        {
            indices.intersection = static_cast<uint32_t>(stages.size());
            stages.push_back(group.intersection->get_stage_create_info());
        }
    }

    //
    // Shader groups
    //

    VkRayTracingShaderGroupCreateInfoKHR group_template{};
    group_template.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group_template.generalShader = VK_SHADER_UNUSED_KHR;
    group_template.closestHitShader = VK_SHADER_UNUSED_KHR;
    group_template.anyHitShader = VK_SHADER_UNUSED_KHR;
    group_template.intersectionShader = VK_SHADER_UNUSED_KHR;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    groups.reserve(1 + miss_shaders.size() + hit_groups.size());

    // Raygen

    {
        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(group_template);
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = 0;
    }

    // Miss groups

    for (uint32_t i = 0; i < miss_shaders.size(); ++i)
    {
        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(group_template);
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = miss_stage_indices[i];
    }

    // Hit groups

    for (uint32_t i = 0; i < hit_groups.size(); ++i)
    {
        const VulkanHitGroup& src = hit_groups[i];
        const HitGroupStageIndices& indices = hit_group_stage_indices[i];

        VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back(group_template);

        if (src.intersection)
        {
            group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
        }
        else
        {
            group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        }

        group.closestHitShader = indices.closest_hit;
        group.anyHitShader = indices.any_hit;
        group.intersectionShader = indices.intersection;
    }

    m_pipeline_layout = VulkanContext.pipeline_layout_cache->get(desc.layout);
    get_push_constant_info_if_exists(desc.layout);

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

    VK_CHECK(vkCreateRayTracingPipelinesKHR(
        VulkanContext.device->handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &create_info, nullptr, &m_pipeline));

    //
    // Create SBT
    //

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& props = VulkanContext.rtx_properties;

    const uint32_t miss_count = static_cast<uint32_t>(miss_shaders.size());
    const uint32_t hit_group_count = static_cast<uint32_t>(hit_groups.size());
    const uint32_t group_count = static_cast<uint32_t>(groups.size());

    const uint32_t handle_size = props.shaderGroupHandleSize;
    const uint32_t handle_alignment = props.shaderGroupHandleAlignment;
    const uint32_t base_alignment = props.shaderGroupBaseAlignment;

    const auto align_up = [](uint32_t value, uint32_t alignment) -> uint32_t {
        MIZU_ASSERT(alignment != 0, "Invalid alignment");

        return (value + alignment - 1) & ~(alignment - 1);
    };

    const uint32_t handle_size_aligned = align_up(handle_size, handle_alignment);

    const uint32_t raygen_stride = align_up(handle_size_aligned, base_alignment);
    const uint32_t miss_stride = handle_size_aligned;
    const uint32_t hit_stride = handle_size_aligned;

    const uint32_t raygen_size = raygen_stride;
    const uint32_t miss_size = miss_count * miss_stride;
    const uint32_t hit_size = hit_group_count * hit_stride;

    const uint32_t raygen_offset = 0;
    const uint32_t miss_offset = align_up(raygen_offset + raygen_size, base_alignment);
    const uint32_t hit_offset = align_up(miss_offset + miss_size, base_alignment);
    const uint32_t sbt_size = hit_offset + hit_size;

    const uint32_t handles_size = group_count * handle_size;

    std::vector<uint8_t> handles(handles_size);

    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
        VulkanContext.device->handle(), m_pipeline, 0, group_count, handles_size, handles.data()));

    const auto get_handle = [&](uint32_t group_index) -> const uint8_t* {
        return handles.data() + group_index * handle_size;
    };

    BufferDescription sbt_desc{};
    sbt_desc.size = sbt_size;
    sbt_desc.usage = BufferUsageBits::RtxShaderBindingTable | BufferUsageBits::HostVisible;
    sbt_desc.name = "SBT_buffer";
    m_sbt_buffer = std::make_unique<VulkanBufferResource>(sbt_desc);

    VkBufferDeviceAddressInfo address_info{};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = m_sbt_buffer->handle();
    const VkDeviceAddress sbt_address = vkGetBufferDeviceAddress(VulkanContext.device->handle(), &address_info);

    m_ray_generation_region.deviceAddress = sbt_address + raygen_offset;
    m_ray_generation_region.stride = raygen_stride;
    m_ray_generation_region.size = raygen_size;

    if (miss_count != 0)
    {
        m_miss_region.deviceAddress = sbt_address + miss_offset;
        m_miss_region.stride = miss_stride;
        m_miss_region.size = miss_size;
    }
    else
    {
        m_miss_region = {};
    }

    if (hit_group_count != 0)
    {
        m_hit_region.deviceAddress = sbt_address + hit_offset;
        m_hit_region.stride = hit_stride;
        m_hit_region.size = hit_size;
    }
    else
    {
        m_hit_region = {};
    }

    std::vector<uint8_t> sbt_data(sbt_size, 0);
    uint32_t group_index = 0;

    memcpy(sbt_data.data() + raygen_offset, get_handle(group_index), handle_size);
    ++group_index;

    uint8_t* miss_dst = sbt_data.data() + miss_offset;
    for (uint32_t i = 0; i < miss_count; ++i)
    {
        memcpy(miss_dst, get_handle(group_index), handle_size);

        ++group_index;

        miss_dst += miss_stride;
    }

    uint8_t* hit_dst = sbt_data.data() + hit_offset;
    for (uint32_t i = 0; i < hit_group_count; ++i)
    {
        memcpy(hit_dst, get_handle(group_index), handle_size);

        ++group_index;

        hit_dst += hit_stride;
    }

    m_sbt_buffer->set_data(sbt_data.data());
}

//
// Other
//

VulkanPipeline::~VulkanPipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
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

std::optional<PushConstantItem> VulkanPipeline::get_push_constant_info() const
{
    return m_push_constant_info;
}

void VulkanPipeline::get_push_constant_info_if_exists(PipelineLayoutHandle handle)
{
    m_push_constant_info = VulkanContext.pipeline_layout_cache->get_push_constant_item(handle);
}

} // namespace Mizu::Vulkan
