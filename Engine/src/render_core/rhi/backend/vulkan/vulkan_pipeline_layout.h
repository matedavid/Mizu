#pragma once

#include <unordered_map>

#include "render_core/rhi/pipeline_layout.h"

#include "render_core/rhi/backend/vulkan/vulkan_core.h"

namespace Mizu::Vulkan
{

using PipelineLayoutId = size_t;

class VulkanPipelineLayoutCache
{
  public:
    VulkanPipelineLayoutCache() = default;
    ~VulkanPipelineLayoutCache();

    VkPipelineLayout create(PipelineLayoutId id, const VkPipelineLayoutCreateInfo& create_info);
    VkPipelineLayout get(PipelineLayoutId id) const;
    bool contains(PipelineLayoutId id) const;

  private:
    std::unordered_map<PipelineLayoutId, VkPipelineLayout> m_cache;
};

VkPipelineLayout create_pipeline_layout(std::span<DescriptorBindingInfo> binding_info);

} // namespace Mizu::Vulkan