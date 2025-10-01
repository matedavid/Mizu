#pragma once

#include <mutex>

#include "render_core/rhi/renderer.h"

namespace Mizu::Vulkan
{

class VulkanBackend : public IBackend
{
  public:
    VulkanBackend() = default;
    ~VulkanBackend() override;

    bool initialize(const RendererConfiguration& config) override;

    void wait_idle() const override;

    RendererCapabilities get_capabilities() const override;

  private:
    mutable std::mutex m_mutex;
};

} // namespace Mizu::Vulkan
