#pragma once

#include <unordered_set>
#include <vector>

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

    uint32_t num_transient_pools;
};

class VulkanDescriptorManager
{
  public:
    VulkanDescriptorManager(const VulkanDescriptorManagerDescription& desc);
    ~VulkanDescriptorManager();

    std::shared_ptr<DescriptorSet> allocate_transient(DescriptorSetLayoutHandle layout);
    void reset_transient(uint32_t pool_idx);

    std::shared_ptr<DescriptorSet> allocate_persistent(DescriptorSetLayoutHandle layout);
    void free_persistent(VkDescriptorSet set) const;

    std::shared_ptr<DescriptorSet> allocate_bindless(DescriptorSetLayoutHandle layout, uint32_t variable_count);

  private:
    std::vector<VkDescriptorPool> m_transient_descriptor_pools{};
    VkDescriptorPool m_persistent_descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorPool m_bindless_descriptor_pool{VK_NULL_HANDLE};

    uint32_t m_current_transient_pool_idx = 0;

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    std::vector<std::unordered_set<VulkanDescriptorSet*>> m_tracked_transient_resources;

    void transient_descriptor_set_created(VulkanDescriptorSet* descriptor_set);
    void transient_descriptor_set_freed(VulkanDescriptorSet* descriptor_set);
#endif

    friend class VulkanDescriptorSet;
};

} // namespace Mizu::Vulkan
