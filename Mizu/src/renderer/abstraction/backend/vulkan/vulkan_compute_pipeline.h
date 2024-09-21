#pragma once

#include <vulkan/vulkan.h>

#include "renderer/abstraction/compute_pipeline.h"

namespace Mizu::Vulkan {

// Forward declaration
class VulkanComputeShader;

class VulkanComputePipeline : public ComputePipeline {
  public:
    VulkanComputePipeline(const Description& desc);

    [[nodiscard]] VkPipeline handle() const { return m_pipeline; }

  private:
    VkPipeline m_pipeline;

    std::shared_ptr<VulkanComputeShader> m_shader;
};

} // namespace Mizu::Vulkan