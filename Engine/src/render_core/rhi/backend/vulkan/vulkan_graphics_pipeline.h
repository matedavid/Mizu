#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/graphics_pipeline.h"

#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/vulkan/vulkan_pipeline.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanShader;
class VulkanFramebuffer;

class VulkanGraphicsPipeline : public GraphicsPipeline, public IVulkanPipeline
{
  public:
    explicit VulkanGraphicsPipeline(const Description& desc);
    ~VulkanGraphicsPipeline() override;

    void push_constant(VkCommandBuffer command_buffer, std::string_view name, uint32_t size, const void* data) const;

    VkPipeline handle() const override { return m_pipeline; }
    VkPipelineLayout get_pipeline_layout() const override { return m_pipeline_layout; }
    VkDescriptorSetLayout get_descriptor_set_layout(uint32_t set) const override
    {
        MIZU_ASSERT(set < m_set_layouts.size(), "Invalid set ({} >= {})", set, m_set_layouts.size());
        return m_set_layouts[set];
    }
    const ShaderGroup& get_shader_group() const override { return m_shader_group; }
    VkPipelineBindPoint get_pipeline_bind_point() const override { return VK_PIPELINE_BIND_POINT_GRAPHICS; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::vector<VkDescriptorSetLayout> m_set_layouts{};

    std::shared_ptr<VulkanShader> m_vertex_shader{};
    std::shared_ptr<VulkanShader> m_fragment_shader{};

    ShaderGroup m_shader_group;
    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer{};

    void get_vertex_input_descriptions(
        VkVertexInputBindingDescription& binding_description,
        std::vector<VkVertexInputAttributeDescription>& attribute_descriptions) const;

    // Rasterization helpers
    static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode);
    static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode);
    static VkFrontFace get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op);

    // Color Blend helpers
    static VkBlendFactor get_blend_factor(ColorBlendState::BlendFactor factor);
    static VkBlendOp get_blend_operation(ColorBlendState::BlendOperation operation);
    static VkColorComponentFlags get_color_component_flags(ColorBlendState::ColorComponentBits bits);
    static VkLogicOp get_logic_operation(ColorBlendState::LogicOperation operation);
};

} // namespace Mizu::Vulkan
