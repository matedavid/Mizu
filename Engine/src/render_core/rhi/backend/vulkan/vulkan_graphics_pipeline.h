#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/graphics_pipeline.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanShader;
class VulkanGraphicsShader;
class VulkanFramebuffer;

class VulkanGraphicsPipeline : public GraphicsPipeline
{
  public:
    explicit VulkanGraphicsPipeline(const Description& desc);
    ~VulkanGraphicsPipeline() override;

    void push_constant(VkCommandBuffer command_buffer, std::string_view name, uint32_t size, const void* data) const;

    [[nodiscard]] VkPipeline handle() const { return m_pipeline; }
    [[nodiscard]] std::shared_ptr<VulkanGraphicsShader> get_shader() const { return m_shader; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    std::vector<VkDescriptorSetLayout> m_set_layouts;

    std::shared_ptr<VulkanShader> m_vertex_shader{};
    std::shared_ptr<VulkanShader> m_fragment_shader{};

    std::shared_ptr<VulkanGraphicsShader> m_shader;
    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer{};

    void get_vertex_input_descriptions(VkVertexInputBindingDescription& binding_description,
                                       std::vector<VkVertexInputAttributeDescription>& attribute_descriptions) const;
    void create_pipeline_layout();

    // Rasterization helpers
    [[nodiscard]] static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode);
    [[nodiscard]] static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode);
    [[nodiscard]] static VkFrontFace get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    [[nodiscard]] static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op);
};

} // namespace Mizu::Vulkan
