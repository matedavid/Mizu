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

    SyncPoint acquire_next_image() override;
    void present(const SyncPoint& rendering_done) override;
    std::shared_ptr<ImageResource> get_image(uint32_t idx) const override;
    uint32_t get_num_images() const override { return m_num_images; }
    uint32_t get_current_image_idx() const override { return m_swapchain->GetCurrentBackBufferIndex(); }

  private:
    IDXGISwapChain3* m_swapchain = nullptr;
    HWND m_window_handle;

    SwapchainDescription m_description{};

    uint32_t m_num_images = 0;
    bool m_allow_tearing = false;

    std::vector<std::shared_ptr<Dx12ImageResource>> m_images;

    void create_swapchain();
    void retrieve_swapchain_images();

    static uint32_t select_num_images(const SwapchainDescription& desc);
    static bool is_tearing_supported();

    void recreate();
    void cleanup();
};

} // namespace Mizu::Dx12