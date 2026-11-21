#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/swapchain.h"
#include "render_core/rhi/synchronization.h"

namespace Mizu
{

class IRhiWindow;

struct SwapchainManagerDescription
{
    std::shared_ptr<IRhiWindow> window;
    ImageFormat format = ImageFormat::R8G8B8A8_UNORM;

    uint32_t desired_image_count = 0;
    PresentMode present_mode = PresentMode::Fifo;
};

class SwapchainManager
{
  public:
    SwapchainManager() = default;

    bool init(const SwapchainManagerDescription& desc);

    void acquire_next_image();
    void present(const SyncPoint& rendering_done);

    std::shared_ptr<ImageResource> get_current_image() const;
    const SyncPoint& get_image_ready() const { return m_image_ready; }

  private:
    std::shared_ptr<Swapchain> m_swapchain{};

    SyncPoint m_image_ready{};
    uint32_t m_image_idx = 0;
};

} // namespace Mizu
