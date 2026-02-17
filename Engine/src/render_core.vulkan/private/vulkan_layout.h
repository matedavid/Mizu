#pragma once

#include <optional>
#include <unordered_map>

#include "render_core/rhi/descriptors.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

uint32_t get_binding_with_offset(uint32_t binding, ShaderResourceType type);

class VulkanDescriptorSetLayoutCache
{
  public:
    ~VulkanDescriptorSetLayoutCache();

    DescriptorSetLayoutHandle create(const DescriptorSetLayoutDescription& desc);
    VkDescriptorSetLayout get(DescriptorSetLayoutHandle handle) const;
    bool contains(DescriptorSetLayoutHandle handle) const;

  private:
    std::unordered_map<DescriptorSetLayoutHandle, VkDescriptorSetLayout> m_cache;
};

class VulkanPipelineLayoutCache
{
  public:
    ~VulkanPipelineLayoutCache();

    PipelineLayoutHandle create(const PipelineLayoutDescription& desc);
    VkPipelineLayout get(PipelineLayoutHandle handle) const;
    bool contains(PipelineLayoutHandle handle) const;

    std::optional<PushConstantItem> get_push_constant_item(PipelineLayoutHandle handle) const;

  private:
    std::unordered_map<PipelineLayoutHandle, VkPipelineLayout> m_cache;
    std::unordered_map<PipelineLayoutHandle, PushConstantItem> m_push_constant_item_cache;
};

} // namespace Mizu::Vulkan