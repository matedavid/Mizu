#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/shader/shader_group.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanShader;
class VulkanFramebuffer;

class VulkanGraphicsPipeline : public GraphicsPipeline
{
  public:
    explicit VulkanGraphicsPipeline(const Description& desc);
    ~VulkanGraphicsPipeline() override;

    void push_constant(VkCommandBuffer command_buffer, std::string_view name, uint32_t size, const void* data) const;

    VkPipelineLayout get_pipeline_layout() const { return m_pipeline_layout; }
    VkPipeline handle() const { return m_pipeline; }

    const ShaderGroup& get_shader_group() const { return m_shader_group; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::vector<VkDescriptorSetLayout> m_set_layouts{};

    std::shared_ptr<VulkanShader> m_vertex_shader{};
    std::shared_ptr<VulkanShader> m_fragment_shader{};

    ShaderGroup m_shader_group;
    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer{};

    void get_vertex_input_descriptions(VkVertexInputBindingDescription& binding_description,
                                       std::vector<VkVertexInputAttributeDescription>& attribute_descriptions) const;
    void create_pipeline_layout();

    // Rasterization helpers
    static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode);
    static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode);
    static VkFrontFace get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op);
};

} // namespace Mizu::Vulkan
