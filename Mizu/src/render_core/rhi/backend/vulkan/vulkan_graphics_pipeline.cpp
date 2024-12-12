#include "vulkan_graphics_pipeline.h"

#include <array>
#include <cassert>

#include "render_core/resources/texture.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu::Vulkan
{

VulkanGraphicsPipeline::VulkanGraphicsPipeline(const Description& desc)
{
    m_shader = std::dynamic_pointer_cast<VulkanGraphicsShader>(desc.shader);
    assert(m_shader != nullptr && "Could not convert Shader to VulkanShader");

    m_target_framebuffer = std::dynamic_pointer_cast<VulkanFramebuffer>(desc.target_framebuffer);
    assert(m_target_framebuffer != nullptr && "Could not convert Framebuffer to VulkanFramebuffer");

    // Shader
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stage = {
        m_shader->get_vertex_stage_create_info(),
        m_shader->get_fragment_stage_create_info(),
    };

    // Vertex input
    const auto binding_description = m_shader->get_vertex_input_binding_description();
    const auto attribute_description = m_shader->get_vertex_input_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_description;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_description.size());
    vertex_input.pVertexAttributeDescriptions = attribute_description.data();

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
    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = VK_FALSE;
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
    // TODO:
    {
        static bool s_color_blending_warning_shown = false;
        if (!s_color_blending_warning_shown)
        {
            MIZU_LOG_WARNING("Vulkan::GraphicsPipeline color blending not implemented");
            s_color_blending_warning_shown = true;
        }
    }
    for (const auto& attachment : desc.target_framebuffer->get_attachments())
    {
        if (ImageUtils::is_depth_format(attachment.image->get_resource()->get_format()))
            continue;

        // TODO: Make blending configurable
        VkPipelineColorBlendAttachmentState state{};
        state.blendEnable = VK_FALSE;
        state.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        color_blend_attachments.push_back(state);
    }

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.logicOpEnable = desc.color_blend.logic_op_enable;
    // TODO: color_blend.logicOp
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
    dynamic_state.dynamicStateCount = dynamic_state_vals.size();
    dynamic_state.pDynamicStates = dynamic_state_vals.data();

    //
    // Create Pipeline
    //
    const auto native_framebuffer = std::dynamic_pointer_cast<VulkanFramebuffer>(desc.target_framebuffer);

    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = 2;
    create_info.pStages = shader_stage.data();
    create_info.pVertexInputState = &vertex_input;
    create_info.pInputAssemblyState = &input_assembly;
    create_info.pTessellationState = &tessellation;
    create_info.pViewportState = &viewport;
    create_info.pRasterizationState = &rasterization;
    create_info.pMultisampleState = &multisample;
    create_info.pDepthStencilState = &depth_stencil;
    create_info.pColorBlendState = &color_blend;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = m_shader->get_pipeline_layout();
    create_info.renderPass = native_framebuffer->get_render_pass();
    create_info.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(VulkanContext.device->handle(), nullptr, 1, &create_info, nullptr, &m_pipeline));
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
{
    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
}

void VulkanGraphicsPipeline::push_constant(VkCommandBuffer command_buffer,
                                           std::string_view name,
                                           uint32_t size,
                                           const void* data)
{
    const auto info = m_shader->get_constant(name);
    MIZU_ASSERT(info.has_value(), "Push constant '{}' not found in GraphicsPipeline", name);

    MIZU_ASSERT(info->size == size,
                "Size of provided data and size of push constant do not match ({} != {})",
                size,
                info->size);

    vkCmdPushConstants(
        command_buffer, m_shader->get_pipeline_layout(), *m_shader->get_constant_stage(name), 0, size, data);
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

} // namespace Mizu::Vulkan
