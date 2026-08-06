#include "dx12_swapchain.h"

#include <algorithm>

#include "base/debug/profiling.h"
#include "render_core/definitions/rhi_window.h"

#include "dx12_context.h"
#include "dx12_image_resource.h"
#include "dx12_synchronization.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

static constexpr uint32_t DEFAULT_SWAPCHAIN_BUFFER_COUNT = 3;
// DXGI_SWAP_EFFECT_FLIP_DISCARD requires at least 2 buffers
static constexpr uint32_t MIN_SWAPCHAIN_BUFFER_COUNT = 2;

Dx12Swapchain::Dx12Swapchain(SwapchainDescription desc) : m_description(std::move(desc))
{
    m_window_handle = (HWND)m_description.window->create_dx12_window_handle();
    m_num_images = select_num_images(m_description);

    create_swapchain();
    retrieve_swapchain_images();
}

uint32_t Dx12Swapchain::select_num_images(const SwapchainDescription& desc)
{
    if (desc.desired_image_count == 0)
        return DEFAULT_SWAPCHAIN_BUFFER_COUNT;

    return std::clamp(desc.desired_image_count, MIN_SWAPCHAIN_BUFFER_COUNT, uint32_t{DXGI_MAX_SWAP_CHAIN_BUFFERS});
}

bool Dx12Swapchain::is_tearing_supported()
{
    IDXGIFactory5* factory5 = nullptr;
    if (FAILED(Dx12Context.factory->QueryInterface(IID_PPV_ARGS(&factory5))))
        return false;

    BOOL allow_tearing = FALSE;
    const HRESULT result =
        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
    factory5->Release();

    return SUCCEEDED(result) && allow_tearing == TRUE;
}

Dx12Swapchain::~Dx12Swapchain()
{
    cleanup();
}

void Dx12Swapchain::acquire_next_image(
    std::shared_ptr<Semaphore> signal_semaphore,
    [[maybe_unused]] std::shared_ptr<Fence> signal_fence)
{
    // In D3D12, Swapchain images are always ready to render at the CurrentBackBufferIndex, therefore there is no need
    // to signal or wait for fences.

    if (signal_semaphore != nullptr)
    {
        Dx12Semaphore& native_signal_semaphore = static_cast<Dx12Semaphore&>(*signal_semaphore);
        native_signal_semaphore.signal(Dx12Context.device->get_graphics_queue());
    }
}

void Dx12Swapchain::present(std::span<std::shared_ptr<Semaphore>>)
{
    MIZU_PROFILE_SCOPED;

    const IRhiWindow& window = *m_description.window;

    DXGI_SWAP_CHAIN_DESC swapchain_desc{};
    DX12_CHECK(m_swapchain->GetDesc(&swapchain_desc));

    if (window.get_width() != swapchain_desc.BufferDesc.Width
        || window.get_height() != swapchain_desc.BufferDesc.Height)
    {
        recreate();
    }

    // DXGI has no direct Mailbox equivalent, but the flip model already discards superseded frames when there are 3 or
    // more buffers, so both Fifo and Mailbox wait for vblank.
    const UINT sync_interval = m_description.present_mode == PresentMode::Immediate ? 0 : 1;
    const UINT present_flags = (m_allow_tearing && sync_interval == 0) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    DX12_CHECK(m_swapchain->Present(sync_interval, present_flags));
}

std::shared_ptr<ImageResource> Dx12Swapchain::get_image(uint32_t idx) const
{
    MIZU_ASSERT(
        idx < m_images.size(), "idx is bigger than the number of swapchain images ({} >= {})", idx, m_images.size());

    const std::shared_ptr<ImageResource> resource = m_images[idx];
    return resource;
}

void Dx12Swapchain::create_swapchain()
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
    swapchain_desc.Width = 0;
    swapchain_desc.Height = 0;
    swapchain_desc.Format = get_dx12_image_format(m_description.format);
    swapchain_desc.Stereo = FALSE;
    swapchain_desc.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = m_num_images;
    swapchain_desc.Scaling = DXGI_SCALING_NONE;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    m_allow_tearing = m_description.present_mode == PresentMode::Immediate && is_tearing_supported();
    swapchain_desc.Flags = m_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    IDXGISwapChain1* swapchain1;
    DX12_CHECK(Dx12Context.factory->CreateSwapChainForHwnd(
        Dx12Context.device->get_graphics_queue(), m_window_handle, &swapchain_desc, nullptr, nullptr, &swapchain1));

    swapchain1->QueryInterface(IID_PPV_ARGS(&m_swapchain));
    swapchain1->Release();

    DX12_CHECK(Dx12Context.factory->MakeWindowAssociation(m_window_handle, DXGI_MWA_NO_ALT_ENTER));
}

void Dx12Swapchain::retrieve_swapchain_images()
{
    MIZU_ASSERT(m_images.empty(), "Image vector should be empty");

    m_images.resize(m_num_images);

    for (uint32_t i = 0; i < m_num_images; ++i)
    {
        ID3D12Resource* back_buffer;
        DX12_CHECK(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));
        back_buffer->SetName(L"SwapchainBackBuffer");

        const uint32_t width = static_cast<uint32_t>(back_buffer->GetDesc().Width);
        const uint32_t height = static_cast<uint32_t>(back_buffer->GetDesc().Height);

        m_images[i] = std::make_shared<Dx12ImageResource>(
            width, height, m_description.format, m_description.usage, back_buffer, false);
    }
}

void Dx12Swapchain::recreate()
{
    Dx12Context.device->wait_idle();
    cleanup();

    create_swapchain();
    retrieve_swapchain_images();
}

void Dx12Swapchain::cleanup()
{
    for (const auto& image : m_images)
    {
        // Can't just rely on destructor because we create the images with m_owns_resources = false
        image->handle()->Release();
    }
    m_images.clear();

    m_swapchain->Release();
}

} // namespace Mizu::Dx12