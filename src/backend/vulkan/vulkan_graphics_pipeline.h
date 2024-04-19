#pragma once

#include <variant>
#include <vector>
#include <vulkan/vulkan.h>

#include "graphics_pipeline.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanShader;
class VulkanFramebuffer;
class VulkanDescriptorPool;
struct VulkanDescriptorInfo;

class VulkanGraphicsPipeline : public GraphicsPipeline {
  public:
    explicit VulkanGraphicsPipeline(const Description& desc);
    ~VulkanGraphicsPipeline() override;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    [[nodiscard]] bool bake() override;

    void add_input(std::string_view name, const std::shared_ptr<Texture2D>& texture) override;

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    std::shared_ptr<VulkanShader> m_shader;
    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer;

    // Descriptor set info
    VkDescriptorSet m_set{VK_NULL_HANDLE};
    std::unique_ptr<VulkanDescriptorPool> m_descriptor_pool;

    using DescriptorInfo = std::variant<VkDescriptorBufferInfo*, VkDescriptorImageInfo*>;
    std::vector<std::pair<VulkanDescriptorInfo, DescriptorInfo>> m_descriptor_info;

    // Rasterization helpers
    [[nodiscard]] static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode);
    [[nodiscard]] static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode);
    [[nodiscard]] static VkFrontFace get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    [[nodiscard]] static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op);
};

} // namespace Mizu::Vulkan
