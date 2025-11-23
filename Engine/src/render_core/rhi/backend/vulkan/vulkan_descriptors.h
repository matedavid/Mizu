#pragma once

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

namespace Mizu::Vulkan
{

class VulkanDescriptorPool
{
  public:
    using PoolSize = std::vector<std::pair<VkDescriptorType, size_t>>;

    VulkanDescriptorPool(PoolSize size, uint32_t max_sets, bool enable_free = true);
    ~VulkanDescriptorPool();

    bool allocate(VkDescriptorSetLayout layout, VkDescriptorSet& set);
    void free(VkDescriptorSet set);

  private:
    VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
    PoolSize m_pool_size;

    uint32_t m_max_sets;
    uint32_t m_allocated_sets{0};
    bool m_enable_free;
};

class VulkanDescriptorLayoutCache
{
  public:
    VulkanDescriptorLayoutCache() = default;
    ~VulkanDescriptorLayoutCache();

    VkDescriptorSetLayout create_descriptor_layout(const VkDescriptorSetLayoutCreateInfo& info);

  private:
    struct DescriptorLayoutInfo
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        bool operator==(const DescriptorLayoutInfo& other) const;
        size_t hash() const;
    };

    struct DescriptorLayoutHash
    {
        size_t operator()(const DescriptorLayoutInfo& info) const { return info.hash(); }
    };

    std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> m_layout_cache;
};

class VulkanDescriptorBuilder
{
  public:
    static VulkanDescriptorBuilder begin(VulkanDescriptorLayoutCache* cache, VulkanDescriptorPool* pool);

    VulkanDescriptorBuilder& bind_buffer(
        uint32_t binding,
        const VkDescriptorBufferInfo* buffer_info,
        VkDescriptorType type,
        VkShaderStageFlags stage_flags,
        uint32_t descriptor_count = 1);

    VulkanDescriptorBuilder& bind_image(
        uint32_t binding,
        const VkDescriptorImageInfo* image_info,
        VkDescriptorType type,
        VkShaderStageFlags stage_flags,
        uint32_t descriptor_count = 1);

    VulkanDescriptorBuilder& bind_sampler(
        uint32_t binding,
        const VkDescriptorImageInfo* image_info,
        VkDescriptorType type,
        VkShaderStageFlags stage_flags,
        uint32_t descriptor_count = 1);

    VulkanDescriptorBuilder& bind_acceleration_structure(
        uint32_t binding,
        const VkWriteDescriptorSetAccelerationStructureKHR* acceleration_structure,
        VkDescriptorType type,
        VkShaderStageFlags stage_flags,
        uint32_t descriptor_count = 1);

    bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
    bool build(VkDescriptorSet& set);

  private:
    VulkanDescriptorLayoutCache* m_cache = nullptr;
    VulkanDescriptorPool* m_pool = nullptr;

    std::vector<VkWriteDescriptorSet> m_writes;
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};

} // namespace Mizu::Vulkan