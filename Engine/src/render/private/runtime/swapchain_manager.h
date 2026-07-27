#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/swapchain.h"

namespace Mizu
{

class Fence;
class IRhiWindow;
class Semaphore;

struct SwapchainManagerDescription
{
    std::shared_ptr<IRhiWindow> window;
    ImageFormat format = ImageFormat::R8G8B8A8_UNORM;

    uint32_t frames_in_flight = 0;

    uint32_t desired_image_count = 0;
    PresentMode present_mode = PresentMode::Fifo;
};

class SwapchainManager
{
  public:
    SwapchainManager() = default;

    bool init(const SwapchainManagerDescription& desc);

    void acquire_next_image(uint32_t frame_in_flight_idx, const std::shared_ptr<Fence>& frame_fence);
    void present();

    std::shared_ptr<ImageResource> get_current_image() const;

    const std::shared_ptr<Semaphore>& get_image_acquired_semaphore() const;
    const std::shared_ptr<Semaphore>& get_render_finished_semaphore() const;

  private:
    std::shared_ptr<Swapchain> m_swapchain{};

    std::vector<std::shared_ptr<Semaphore>> m_image_acquired_semaphores{};
    std::vector<std::shared_ptr<Semaphore>> m_render_finished_semaphores{};

    uint32_t m_frame_in_flight_idx = 0;
    uint32_t m_image_idx = 0;

    void create_render_finished_semaphores();
};

} // namespace Mizu
