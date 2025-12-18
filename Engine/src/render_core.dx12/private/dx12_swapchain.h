#pragma once

#include "render_core/rhi/swapchain.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

// Forward declarations
class Dx12ImageResource;

class Dx12Swapchain : public Swapchain
{
  public:
    Dx12Swapchain(SwapchainDescription desc);
    ~Dx12Swapchain() override;

    void acquire_next_image(std::shared_ptr<Semaphore> signal_semaphore, std::shared_ptr<Fence> signal_fence) override;
    void present(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) override;
    std::shared_ptr<Texture2D> get_image(uint32_t idx) const override;
    uint32_t get_current_image_idx() const override { return m_swapchain->GetCurrentBackBufferIndex(); }

  private:
    IDXGISwapChain3* m_swapchain = nullptr;
    HWND m_window_handle;
    uint32_t m_current_image_idx = 0;

    SwapchainDescription m_description{};

    std::vector<std::shared_ptr<Dx12ImageResource>> m_images;

    void create_swapchain();
    void retrieve_swapchain_images();
};

} // namespace Mizu::Dx12