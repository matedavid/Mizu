#pragma once

#include "renderer/abstraction/renderer.h"

namespace Mizu::Vulkan {

class VulkanBackend : public IBackend {
  public:
    VulkanBackend() = default;
    ~VulkanBackend() override;

    [[nodiscard]] bool initialize(const RendererConfiguration& config) override;
};

} // namespace Mizu::Vulkan