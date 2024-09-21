#pragma once

#include "renderer/abstraction/compute_pipeline.h"

namespace Mizu::Vulkan {

class VulkanComputePipeline : public ComputePipeline {
  public:
    VulkanComputePipeline(const Description& desc);
};

} // namespace Mizu::Vulkan