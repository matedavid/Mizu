#pragma once

#include <unordered_set>

#include "render_core/rhi/descriptors.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

class VulkanDescriptorManager;

class VulkanDescriptorSet : public DescriptorSet
{
  public:
    VulkanDescriptorSet(
        VkDescriptorSet descriptor_set,
        VulkanDescriptorManager& manager,
        DescriptorSetAllocationType type);
    ~VulkanDescriptorSet() override;

    void update(std::span<const WriteDescriptor> writes, uint32_t array_offset = 0) override;

    VkDescriptorSet handle() const { return m_descriptor_set; }

  private:
    VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};
    VulkanDescriptorManager& m_manager;
    DescriptorSetAllocationType m_type;
};

struct VulkanDescriptorManagerDescription
{
    std::span<VkDescriptorPoolSize> transient_pool_sizes;
    std::span<VkDescriptorPoolSize> persistent_pool_sizes;
    std::span<VkDescriptorPoolSize> bindless_pool_sizes;
};

class VulkanDescriptorManager
{
  public:
    VulkanDescriptorManager(const VulkanDescriptorManagerDescription& desc);
    ~VulkanDescriptorManager();

    std::shared_ptr<DescriptorSet> allocate_transient(DescriptorSetLayoutHandle layout);
    void reset_transient();

    std::shared_ptr<DescriptorSet> allocate_persistent(DescriptorSetLayoutHandle layout);
    void free_persistent(VkDescriptorSet set) const;

    std::shared_ptr<DescriptorSet> allocate_bindless(DescriptorSetLayoutHandle layout, uint32_t variable_count);

  private:
    VkDescriptorPool m_transient_descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorPool m_persistent_descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorPool m_bindless_descriptor_pool{VK_NULL_HANDLE};

    VkDescriptorSetLayout get_descriptor_set_layout(
        std::span<const DescriptorItem> layout,
        DescriptorSetAllocationType type);

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    std::unordered_set<VulkanDescriptorSet*> m_tracked_transient_resources;

    void transient_descriptor_set_created(VulkanDescriptorSet* descriptor_set);
    void transient_descriptor_set_freed(VulkanDescriptorSet* descriptor_set);
#endif

    friend class VulkanDescriptorSet;
};

} // namespace Mizu::Vulkan