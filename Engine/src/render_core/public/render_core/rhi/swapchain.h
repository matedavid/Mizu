#pragma once

#include <memory>
#include <span>

#include "render_core/rhi/image_resource.h"

namespace Mizu
{

// Forward declarations
class Fence;
class IRhiWindow;
class Semaphore;

enum class PresentMode
{
    // Waits for the next vertical blank, no tearing. The only mode guaranteed to be supported.
    Fifo,
    // Waits for the next vertical blank but replaces already queued images with newer ones, no tearing.
    Mailbox,
    // Does not wait for the next vertical blank, may tear.
    Immediate,
};

struct SwapchainDescription
{
    std::shared_ptr<IRhiWindow> window{};
    ImageFormat format{};

    // == 0 means let the Rhi decide.
    uint32_t desired_image_count = 0;

    // If the requested mode is not supported, the backend falls back to PresentMode::Fifo.
    PresentMode present_mode = PresentMode::Fifo;

    ImageUsageBits usage = ImageUsageBits::Attachment | ImageUsageBits::TransferDst;
};

class Swapchain
{
  public:
    virtual ~Swapchain() = default;

    virtual void acquire_next_image(
        std::shared_ptr<Semaphore> signal_semaphore,
        std::shared_ptr<Fence> signal_fence) = 0;
    virtual void present(std::span<std::shared_ptr<Semaphore>> wait_semaphores) = 0;
    virtual std::shared_ptr<ImageResource> get_image(uint32_t idx) const = 0;

    virtual uint32_t get_num_images() const = 0;
    virtual uint32_t get_current_image_idx() const = 0;
};

} // namespace Mizu
