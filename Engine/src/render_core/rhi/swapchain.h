#pragma once

#include <memory>
#include <vector>

namespace Mizu
{

// Forward declarations
class Texture2D;
class Semaphore;
class Fence;
class IRHIWindow;

class Swapchain
{
  public:
    virtual ~Swapchain() = default;

    static std::shared_ptr<Swapchain> create(std::shared_ptr<IRHIWindow> window);

    virtual void acquire_next_image(
        std::shared_ptr<Semaphore> signal_semaphore,
        std::shared_ptr<Fence> signal_fence) = 0;
    virtual void present(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) = 0;
    virtual std::shared_ptr<Texture2D> get_image(uint32_t idx) const = 0;
    virtual uint32_t get_current_image_idx() const = 0;
};

} // namespace Mizu