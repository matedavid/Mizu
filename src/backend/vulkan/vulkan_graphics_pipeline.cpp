#include "vulkan_graphics_pipeline.h"

#include <array>
#include <cassert>

#include "utility/logging.h"

#include "backend/vulkan/vk_core.h"
#include "backend/vulkan/vulkan_buffers.h"
#include "backend/vulkan/vulkan_command_buffer.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_descriptors.h"
#include "backend/vulkan/vulkan_framebuffer.h"
#include "backend/vulkan/vulkan_image.h"
#include "backend/vulkan/vulkan_shader.h"
#include "backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

constexpr uint32_t GRAPHICS_PIPELINE_DESCRIPTOR_SET = 1;

VulkanGraphicsPipeline::VulkanGraphicsPipeline(const Description& desc) {
    m_shader = std::dynamic_pointer_cast<VulkanShader>(desc.shader);
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
    for (const auto& attachment : desc.target_framebuffer->get_attachments()) {
        if (ImageUtils::is_depth_format(attachment.image->get_format()))
            continue;

        // TODO: Make blending configurable
        VkPipelineColorBlendAttachmentState state{};
        state.blendEnable = VK_FALSE;
        state.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        color_blend_attachments.push_back(state);
    }

    // TODO:
    MIZU_LOG_WARNING("Vulkan::GraphicsPipeline color blending not implemented");

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

    //
    // Retrieve descriptor info
    //
    std::unordered_map<VkDescriptorType, uint32_t> pool_size_map;

    const auto descriptors = m_shader->get_descriptors_in_set(GRAPHICS_PIPELINE_DESCRIPTOR_SET);
    for (const auto& descriptor : descriptors) {
        m_descriptor_info.insert({descriptor.name, std::nullopt});

        auto it = pool_size_map.find(descriptor.type);
        if (it == pool_size_map.end())
            it = pool_size_map.insert({descriptor.type, 0}).first;
        it->second += 1;
    }

    VulkanDescriptorPool::PoolSize pool_size{pool_size_map.begin(), pool_size_map.end()};
    m_descriptor_pool = std::make_unique<VulkanDescriptorPool>(pool_size, 1);
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
    for (const auto& [name, info] : m_descriptor_info) {
        if (!info.has_value())
            continue;
        std::visit([]<typename T>(T* ptr) { delete ptr; }, *info);
    }

    vkDestroyPipeline(VulkanContext.device->handle(), m_pipeline, nullptr);
}

void VulkanGraphicsPipeline::bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const {
    const auto& native = std::dynamic_pointer_cast<IVulkanCommandBuffer>(command_buffer);

    // Bind pipeline
    vkCmdBindPipeline(native->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    // Bind descriptor set
    if (m_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(native->handle(),
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_shader->get_pipeline_layout(),
                                GRAPHICS_PIPELINE_DESCRIPTOR_SET,
                                1,
                                &m_set,
                                0,
                                nullptr);
    }

    // TODO: Think of moving to a different place
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_target_framebuffer->get_width());
    viewport.height = static_cast<float>(m_target_framebuffer->get_height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_target_framebuffer->get_width(), m_target_framebuffer->get_height()};

    vkCmdSetViewport(native->handle(), 0, 1, &viewport);
    vkCmdSetScissor(native->handle(), 0, 1, &scissor);
}

bool VulkanGraphicsPipeline::bake() {
    if (m_set != VK_NULL_HANDLE)
        return true;

    auto builder = VulkanDescriptorBuilder::begin(VulkanContext.layout_cache.get(), m_descriptor_pool.get());

    bool all_descriptors_have_value = true;
    for (const auto& [name, info] : m_descriptor_info) {
        if (!info.has_value()) {
            MIZU_LOG_ERROR("GraphicsPipeline input '{}' does not contain value", name);
            all_descriptors_have_value = false;
            continue;
        }

        const auto desc_info = m_shader->get_descriptor_info(name);
        assert(desc_info.has_value());

        if (std::holds_alternative<VkDescriptorImageInfo*>(*info)) {
            const auto write = std::get<VkDescriptorImageInfo*>(*info);
            builder =
                builder.bind_image(desc_info->binding, write, desc_info->type, desc_info->stage, desc_info->count);
        } else if (std::holds_alternative<VkDescriptorBufferInfo*>(*info)) {
            const auto write = std::get<VkDescriptorBufferInfo*>(*info);
            builder =
                builder.bind_buffer(desc_info->binding, write, desc_info->type, desc_info->stage, desc_info->count);
        }
    }

    if (!all_descriptors_have_value)
        return false;

    return builder.build(m_set);
}

void VulkanGraphicsPipeline::add_input(std::string_view name, const std::shared_ptr<Texture2D>& texture) {
    const auto info = get_descriptor_info(name, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "Texture2D");
    if (!info.has_value())
        return;

    const auto native_texture = std::dynamic_pointer_cast<VulkanTexture2D>(texture);
    const auto native_image = native_texture->get_image();

    auto* descriptor = new VkDescriptorImageInfo{};
    descriptor->imageView = native_image->get_image_view();
    descriptor->sampler = native_texture->get_sampler();
    descriptor->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto it = m_descriptor_info.find(info->name);
    assert(it != m_descriptor_info.end());
    it->second = descriptor;
}

void VulkanGraphicsPipeline::add_input(std::string_view name, const std::shared_ptr<UniformBuffer>& ub) {
    const auto info = get_descriptor_info(name, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "UniformBuffer");
    if (!info.has_value())
        return;

    if (info->size != ub->size()) {
        MIZU_LOG_WARNING("Size of descriptor Uniform Buffer and provided Uniform Buffer do not match ({} != {})",
                         info->size,
                         ub->size());
    }

    const auto native_ubo = std::dynamic_pointer_cast<VulkanUniformBuffer>(ub);

    auto* descriptor = new VkDescriptorBufferInfo{};
    descriptor->buffer = native_ubo->handle();
    descriptor->range = native_ubo->size();
    descriptor->offset = 0;

    auto it = m_descriptor_info.find(info->name);
    assert(it != m_descriptor_info.end());
    it->second = descriptor;
}

bool VulkanGraphicsPipeline::push_constant(const std::shared_ptr<ICommandBuffer>& command_buffer,
                                           std::string_view name,
                                           uint32_t size,
                                           const void* data) {
    const auto native_cb = std::dynamic_pointer_cast<IVulkanCommandBuffer>(command_buffer);

    const auto info = m_shader->get_push_constant_info(name);
    if (!info.has_value()) {
        MIZU_LOG_ERROR("Push constant '{}' not found in GraphicsPipeline", name);
        return false;
    }

    if (info->size != size) {
        MIZU_LOG_ERROR("Size of provided data and size of push constant do not match ({} != {})", size, info->size);
        return false;
    }

    vkCmdPushConstants(native_cb->handle(), m_shader->get_pipeline_layout(), info->stage, 0, size, data);
    return true;
}

std::optional<VulkanDescriptorInfo> VulkanGraphicsPipeline::get_descriptor_info(
    std::string_view name,
    VkDescriptorType type,
    [[maybe_unused]] std::string_view type_name) const {
    const auto info = m_shader->get_descriptor_info(name);
    if (!info.has_value()) {
        MIZU_LOG_WARNING("Property '{}' not found in GraphicsPipeline", name);
        return std::nullopt;
    }

    if (info->type != type) {
        MIZU_LOG_WARNING("Property '{}' is not of type {}", name, type_name);
        return std::nullopt;
    }

    if (info->set != GRAPHICS_PIPELINE_DESCRIPTOR_SET) {
        MIZU_LOG_WARNING("Property '{}' has descriptor set {}, but GraphicsPipeline's descriptor set is {}",
                         name,
                         info->set,
                         GRAPHICS_PIPELINE_DESCRIPTOR_SET);
        return std::nullopt;
    }

    return *info;
}

VkPolygonMode VulkanGraphicsPipeline::get_polygon_mode(RasterizationState::PolygonMode mode) {
    using PolygonMode = RasterizationState::PolygonMode;

    switch (mode) {
    case PolygonMode::Fill:
        return VK_POLYGON_MODE_FILL;
    case PolygonMode::Line:
        return VK_POLYGON_MODE_LINE;
    case PolygonMode::Point:
        return VK_POLYGON_MODE_POINT;
    }
}

VkCullModeFlags VulkanGraphicsPipeline::get_cull_mode(RasterizationState::CullMode mode) {
    using CullMode = RasterizationState::CullMode;

    switch (mode) {
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

VkFrontFace VulkanGraphicsPipeline::get_front_face(RasterizationState::FrontFace mode) {
    using FrontFace = RasterizationState::FrontFace;

    switch (mode) {
    case FrontFace::CounterClockwise:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case FrontFace::ClockWise:
        return VK_FRONT_FACE_CLOCKWISE;
    }
}

VkCompareOp VulkanGraphicsPipeline::get_depth_compare_op(DepthStencilState::DepthCompareOp op) {
    using DepthCompareOp = DepthStencilState::DepthCompareOp;

    switch (op) {
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
