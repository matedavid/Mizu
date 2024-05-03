#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>

#include "resource_group.h"

namespace Mizu::Vulkan {

class VulkanResourceGroup : public ResourceGroup {
  public:
    VulkanResourceGroup() = default;
    ~VulkanResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) override;
    void add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) override;

  private:
    VkDescriptorSet m_set{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};

    std::unordered_map<std::string, VkDescriptorImageInfo> m_image_info;
    std::unordered_map<std::string, VkDescriptorBufferInfo> m_buffer_info;
};

} // namespace Mizu::Vulkan
