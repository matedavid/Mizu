#pragma once

#include <cassert>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "render_core/rhi/resource_group.h"
#include "render_core/shader/shader_properties.h"

#include "utility/assert.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanImageResourceView;
class VulkanBufferResource;
class VulkanSamplerState;
class VulkanTopLevelAccelerationStructure;
class VulkanDescriptorPool;
class VulkanShaderBase;
struct VulkanDescriptorInfo;

class VulkanResourceGroup : public ResourceGroup
{
  public:
    VulkanResourceGroup(ResourceGroupLayout layout);
    ~VulkanResourceGroup() override = default;

    size_t get_hash() const override;

    VkDescriptorSet get_descriptor_set() const
    {
        MIZU_ASSERT(m_descriptor_set != VK_NULL_HANDLE, "ResourceGroup has not been baked");
        return m_descriptor_set;
    }

    VkDescriptorSetLayout get_descriptor_set_layout() const
    {
        MIZU_ASSERT(m_descriptor_set_layout != VK_NULL_HANDLE, "ResourceGroup has not been baked");
        return m_descriptor_set_layout;
    }

  private:
    VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};

    std::shared_ptr<VulkanDescriptorPool> m_descriptor_pool;

    ResourceGroupLayout m_layout;
};

} // namespace Mizu::Vulkan
