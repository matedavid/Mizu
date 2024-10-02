#pragma once

#include "renderer/abstraction/renderer.h"

namespace Mizu::Vulkan {

class VulkanBackend : public IBackend {
  public:
    VulkanBackend() = default;
    ~VulkanBackend() override;

    [[nodiscard]] bool initialize(const RendererConfiguration& config) override;

    void wait_idle() const override;
};

} // namespace Mizu::Vulkan
