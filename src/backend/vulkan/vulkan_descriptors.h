#pragma once

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

namespace Mizu::Vulkan {

class VulkanDescriptorPool {
  public:
    using PoolSize = std::vector<std::pair<VkDescriptorType, uint32_t>>;

    VulkanDescriptorPool(PoolSize size, uint32_t num_sets);
    ~VulkanDescriptorPool();

    [[nodiscard]] bool allocate(VkDescriptorSetLayout layout, VkDescriptorSet& set);

  private:
    VkDescriptorPool m_descriptor_pool;
    PoolSize m_pool_size;

    uint32_t m_max_sets;
    uint32_t m_allocated_sets{0};
};

class VulkanDescriptorLayoutCache {
  public:
    VulkanDescriptorLayoutCache() = default;
    ~VulkanDescriptorLayoutCache();

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_layout(const VkDescriptorSetLayoutCreateInfo& info);

  private:
    struct DescriptorLayoutInfo {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        [[nodiscard]] bool operator==(const DescriptorLayoutInfo& other) const;
        [[nodiscard]] size_t hash() const;
    };

    struct DescriptorLayoutHash {
        size_t operator()(const DescriptorLayoutInfo& info) const { return info.hash(); }
    };

    std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> m_layout_cache;
};

class VulkanDescriptorBuilder {
  public:
    static VulkanDescriptorBuilder begin(VulkanDescriptorLayoutCache* cache, VulkanDescriptorPool* pool);

    [[nodiscard]] VulkanDescriptorBuilder& bind_buffer(uint32_t binding,
                                                       const VkDescriptorBufferInfo* buffer_info,
                                                       VkDescriptorType type,
                                                       VkShaderStageFlags stage_flags,
                                                       uint32_t descriptor_count = 1);

    [[nodiscard]] VulkanDescriptorBuilder& bind_image(uint32_t binding,
                                                      const VkDescriptorImageInfo* image_info,
                                                      VkDescriptorType type,
                                                      VkShaderStageFlags stage_flags,
                                                      uint32_t descriptor_count = 1);

    [[nodiscard]] bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
    [[nodiscard]] bool build(VkDescriptorSet& set);

  private:
    VulkanDescriptorLayoutCache* m_cache;
    VulkanDescriptorPool* m_pool;

    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

} // namespace Mizu::Vulkan