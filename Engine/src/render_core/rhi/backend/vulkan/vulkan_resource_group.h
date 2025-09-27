#pragma once

#include <vulkan/vulkan.h>

#include "render_core/rhi/resource_group.h"

namespace Mizu::Vulkan
{

class VulkanResourceGroup : public ResourceGroup
{
  public:
    VulkanResourceGroup(ResourceGroupBuilder builder);
    ~VulkanResourceGroup() override;

    size_t get_hash() const override;

    VkDescriptorSet get_descriptor_set() const { return m_descriptor_set; }
    VkDescriptorSetLayout get_descriptor_set_layout() const { return m_descriptor_set_layout; }

  private:
    VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};

    ResourceGroupBuilder m_builder;
};

} // namespace Mizu::Vulkan
