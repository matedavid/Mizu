#pragma once

#include <optional>
#include <unordered_map>
#include <variant>
#include <vulkan/vulkan.h>

#include "graphics_pipeline.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanShader;
class VulkanTexture2D;
class VulkanUniformBuffer;
class VulkanFramebuffer;
class VulkanDescriptorPool;
struct VulkanDescriptorInfo;

class VulkanGraphicsPipeline : public GraphicsPipeline {
  public:
    explicit VulkanGraphicsPipeline(const Description& desc);
    ~VulkanGraphicsPipeline() override;

    [[nodiscard]] bool push_constant(const std::shared_ptr<ICommandBuffer>& command_buffer,
                                     std::string_view name,
                                     uint32_t size,
                                     const void* data) override;

    [[nodiscard]] VkPipeline handle() const { return m_pipeline; }
    [[nodiscard]] std::shared_ptr<VulkanShader> get_shader() const { return m_shader; }

  private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    std::shared_ptr<VulkanShader> m_shader;
    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer;

    // Descriptor set info
    VkDescriptorSet m_set{VK_NULL_HANDLE};
    std::unique_ptr<VulkanDescriptorPool> m_descriptor_pool;

    using DescriptorInfoT = std::variant<VkDescriptorImageInfo*, VkDescriptorBufferInfo*>;
    std::unordered_map<std::string, std::optional<DescriptorInfoT>> m_descriptor_info;

    [[nodiscard]] std::optional<VulkanDescriptorInfo> get_descriptor_info(std::string_view name,
                                                                          VkDescriptorType type,
                                                                          std::string_view type_name) const;

    // Rasterization helpers
    [[nodiscard]] static VkPolygonMode get_polygon_mode(RasterizationState::PolygonMode mode);
    [[nodiscard]] static VkCullModeFlags get_cull_mode(RasterizationState::CullMode mode);
    [[nodiscard]] static VkFrontFace get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    [[nodiscard]] static VkCompareOp get_depth_compare_op(DepthStencilState::DepthCompareOp op);
};

} // namespace Mizu::Vulkan
