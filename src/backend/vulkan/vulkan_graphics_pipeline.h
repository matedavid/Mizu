#pragma once

#include <vulkan/vulkan.h>

#include "graphics_pipeline.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanShader;

class VulkanGraphicsPipeline : public GraphicsPipeline {
  public:
    explicit VulkanGraphicsPipeline(const Description& desc);
    ~VulkanGraphicsPipeline() override;

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    std::shared_ptr<VulkanShader> m_shader;

    // Rasterization helpers
    [[nodiscard]] static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode);
    [[nodiscard]] static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode);
    [[nodiscard]] static VkFrontFace get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    [[nodiscard]] static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op);
};

} // namespace Mizu::Vulkan
