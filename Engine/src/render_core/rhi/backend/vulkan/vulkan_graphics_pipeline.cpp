#include "vulkan_graphics_pipeline.h"

#include <array>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/resources/texture.h"
#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu::Vulkan
{

VulkanGraphicsPipeline::VulkanGraphicsPipeline(const Description& desc)
{
    m_vertex_shader = std::dynamic_pointer_cast<VulkanShader>(desc.vertex_shader);
    m_fragment_shader = std::dynamic_pointer_cast<VulkanShader>(desc.fragment_shader);

    MIZU_ASSERT(
        m_vertex_shader != nullptr && m_vertex_shader->get_type() == ShaderType::Vertex,
        "No vertex shader provided in GraphicsPipeline");
    MIZU_ASSERT(
        m_fragment_shader != nullptr && m_fragment_shader->get_type() == ShaderType::Fragment,
        "No fragment shader provided in GraphicsPipeline");

    m_target_framebuffer = std::dynamic_pointer_cast<VulkanFramebuffer>(desc.target_framebuffer);
    MIZU_ASSERT(m_target_framebuffer != nullptr, "Could not convert Framebuffer to VulkanFramebuffer");

    // Shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
        m_vertex_shader->get_stage_create_info(),
        m_fragment_shader->get_stage_create_info(),
    };

    // Pipeline layout
    create_pipeline_layout();

    // Vertex input
    VkVertexInputBindingDescription binding_description{};
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

    get_vertex_input_descriptions(binding_description, attribute_descriptions);

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

    const std::vector<Framebuffer::Attachment>& attachments = desc.target_framebuffer->get_attachments();
    for (uint32_t i = 0; i < attachments.size(); ++i)
    {
        const Framebuffer::Attachment& attachment = attachments[i];
        if (ImageUtils::is_depth_format(attachment.image_view->get_format()))
            continue;

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
    const auto native_framebuffer = std::dynamic_pointer_cast<VulkanFramebuffer>(desc.target_framebuffer);

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
    create_info.renderPass = native_framebuffer->get_render_pass();
    create_info.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(VulkanContext.device->handle(), nullptr, 1, &create_info, nullptr, &m_pipeline));
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(VulkanContext.device->handle(), m_pipeline_layout, nullptr);
}

void VulkanGraphicsPipeline::push_constant(
    VkCommandBuffer command_buffer,
    std::string_view name,
    uint32_t size,
    const void* data) const
{
    [[maybe_unused]] const ShaderPushConstant& info = m_shader_group.get_constant_info(std::string(name));
    MIZU_ASSERT(
        info.size == size, "Size of provided data and size of push constant do not match ({} != {})", size, info.size);

    const VkShaderStageFlags stage =
        VulkanShader::get_vulkan_shader_stage_bits(m_shader_group.get_resource_stage_bits(std::string(name)));

    vkCmdPushConstants(command_buffer, m_pipeline_layout, stage, 0, size, data);
}

void VulkanGraphicsPipeline::get_vertex_input_descriptions(
    VkVertexInputBindingDescription& binding_description,
    std::vector<VkVertexInputAttributeDescription>& attribute_descriptions) const
{
    MIZU_ASSERT(m_vertex_shader != nullptr, "No vertex shader available");

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

    uint32_t stride = 0;
    for (const ShaderInputOutput& input_var : m_vertex_shader->get_reflection().get_inputs())
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
}

void VulkanGraphicsPipeline::create_pipeline_layout()
{
    // Gather resources

    m_shader_group = ShaderGroup();
    m_shader_group.add_shader(*m_vertex_shader);
    m_shader_group.add_shader(*m_fragment_shader);

    // Create pipeline layout

    m_set_layouts.clear();
    std::vector<VkPushConstantRange> push_constant_ranges;

    for (uint32_t set = 0; set < m_shader_group.get_max_set(); ++set)
    {
        const std::vector<ShaderResource>& parameters = m_shader_group.get_parameters_in_set(set);

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        layout_bindings.reserve(parameters.size());

        for (const ShaderResource& parameter : parameters)
        {
            VkDescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = parameter.binding_info.binding;
            layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(parameter.value);
            layout_binding.descriptorCount = 1;
            layout_binding.stageFlags =
                VulkanShader::get_vulkan_shader_stage_bits(m_shader_group.get_resource_stage_bits(parameter.name));
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

    for (const ShaderPushConstant& constant : m_shader_group.get_constants())
    {
        VkPushConstantRange range{};
        range.stageFlags =
            VulkanShader::get_vulkan_shader_stage_bits(m_shader_group.get_resource_stage_bits(constant.name));
        range.offset = 0;
        range.size = static_cast<uint32_t>(constant.size);

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

VkPolygonMode VulkanGraphicsPipeline::get_polygon_mode(RasterizationState::PolygonMode mode)
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

VkCullModeFlags VulkanGraphicsPipeline::get_cull_mode(RasterizationState::CullMode mode)
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

VkFrontFace VulkanGraphicsPipeline::get_front_face(RasterizationState::FrontFace mode)
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

VkCompareOp VulkanGraphicsPipeline::get_depth_compare_op(DepthStencilState::DepthCompareOp op)
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

VkBlendFactor VulkanGraphicsPipeline::get_blend_factor(ColorBlendState::BlendFactor factor)
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

} // namespace Mizu::Vulkan

VkBlendOp VulkanGraphicsPipeline::get_blend_operation(ColorBlendState::BlendOperation operation)
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
}

VkColorComponentFlags VulkanGraphicsPipeline::get_color_component_flags(ColorBlendState::ColorComponentBits bits)
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

VkLogicOp VulkanGraphicsPipeline::get_logic_operation(ColorBlendState::LogicOperation operation)
{
    using LogicOperation = ColorBlendState::LogicOperation;

    switch (operation)
    {
    case LogicOperation::Clear:
        return VK_LOGIC_OP_CLEAR;
    }

    MIZU_UNREACHABLE("Unimplemented or Invalid LogicOperation");
}

} // namespace Mizu::Vulkan
