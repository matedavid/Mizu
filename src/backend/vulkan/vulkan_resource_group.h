#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "resource_group.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanDescriptorPool;
class VulkanTexture2D;
class VulkanUniformBuffer;
struct VulkanDescriptorInfo;

class VulkanResourceGroup : public ResourceGroup {
  public:
    VulkanResourceGroup() = default;
    ~VulkanResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) override;
    void add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) override;

    [[nodiscard]] bool bake(const std::shared_ptr<Shader>& shader, uint32_t set) override;

  private:
    VkDescriptorSet m_set{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};

    std::shared_ptr<VulkanDescriptorPool> m_descriptor_pool{};

    std::unordered_map<std::string, std::shared_ptr<VulkanTexture2D>> m_image_info;
    std::unordered_map<std::string, std::shared_ptr<VulkanUniformBuffer>> m_buffer_info;
};

} // namespace Mizu::Vulkan
