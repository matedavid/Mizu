#pragma once

#include <memory>
#include <vector>

#include "mizu_render_core_module.h"

namespace Mizu
{

// Forward declarations
class Fence;
class ImageResource;
class IRHIWindow;
class Semaphore;
enum class ImageFormat;

struct SwapchainDescription
{
    std::shared_ptr<IRHIWindow> window;
    ImageFormat format;
};

class Swapchain
{
  public:
    virtual ~Swapchain() = default;

    virtual void acquire_next_image(
        std::shared_ptr<Semaphore> signal_semaphore,
        std::shared_ptr<Fence> signal_fence) = 0;
    virtual void present(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) = 0;
    virtual std::shared_ptr<ImageResource> get_image(uint32_t idx) const = 0;
    virtual uint32_t get_current_image_idx() const = 0;
};

} // namespace Mizu