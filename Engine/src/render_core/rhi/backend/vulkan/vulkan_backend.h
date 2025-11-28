#pragma once

#include <mutex>

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_core.h"

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

    void initialize_rtx(VkDevice device);
};

} // namespace Mizu::Vulkan
