#pragma once

#include <optional>
#include <unordered_map>
#include <variant>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/graphics_pipeline.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanGraphicsShader;
class VulkanFramebuffer;

class VulkanGraphicsPipeline : public GraphicsPipeline {
  public:
    explicit VulkanGraphicsPipeline(const Description& desc);
    ~VulkanGraphicsPipeline() override;

    void push_constant(VkCommandBuffer command_buffer, std::string_view name, uint32_t size, const void* data);

    [[nodiscard]] VkPipeline handle() const { return m_pipeline; }
    [[nodiscard]] std::shared_ptr<VulkanGraphicsShader> get_shader() const { return m_shader; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    std::shared_ptr<VulkanGraphicsShader> m_shader;
    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer;

    // Rasterization helpers
    [[nodiscard]] static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode);
    [[nodiscard]] static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode);
    [[nodiscard]] static VkFrontFace get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    [[nodiscard]] static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op);
};

} // namespace Mizu::Vulkan
