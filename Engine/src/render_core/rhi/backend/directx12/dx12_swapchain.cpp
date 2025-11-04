#include "dx12_swapchain.h"

#include "render_core/resources/texture.h"
#include "render_core/rhi/rhi_window.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_image_resource.h"

namespace Mizu::Dx12
{

static constexpr uint32_t SWAPCHAIN_BUFFER_COUNT = 3;
static constexpr ImageFormat SWAPCHAIN_FORMAT = ImageFormat::BGRA8_UNORM;

Dx12Swapchain::Dx12Swapchain(std::shared_ptr<IRHIWindow> window) : m_window(std::move(window))
{
    m_window_handle = m_window->create_dx12_window_handle();
    DX12_CHECK(Dx12Context.factory->MakeWindowAssociation(m_window_handle, DXGI_MWA_NO_ALT_ENTER));

    create_swapchain();
    retrieve_swapchain_images();
}

Dx12Swapchain::~Dx12Swapchain()
{
    m_swapchain->Release();
}

void Dx12Swapchain::acquire_next_image(
    [[maybe_unused]] std::shared_ptr<Semaphore> signal_semaphore,
    [[maybe_unused]] std::shared_ptr<Fence> signal_fence)
{
    // In D3D12, Swapchain images are always ready to render at the CurrentBackBufferIndex, therefore there is no need
    // to signal or wait for fences.
}

void Dx12Swapchain::present([[maybe_unused]] const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores)
{
    DX12_CHECK(m_swapchain->Present(0, 0));
}

std::shared_ptr<Texture2D> Dx12Swapchain::get_image(uint32_t idx) const
{
    MIZU_ASSERT(
        idx < m_images.size(), "idx is bigger than the number of swapchain images ({} >= {})", idx, m_images.size());

    const std::shared_ptr<ImageResource> resource = m_images[idx];
    return std::make_shared<Texture2D>(resource);
}

void Dx12Swapchain::create_swapchain()
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
    swapchain_desc.Width = 0;
    swapchain_desc.Height = 0;
    swapchain_desc.Format = Dx12ImageResource::get_dx12_image_format(
        SWAPCHAIN_FORMAT); // TODO: Should make configurable instead of const variable
    swapchain_desc.Stereo = FALSE;
    swapchain_desc.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = SWAPCHAIN_BUFFER_COUNT; // TODO: Should make configurable instead of const variable
    swapchain_desc.Scaling = DXGI_SCALING_NONE;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapchain_desc.Flags = 0;

    IDXGISwapChain1* swapchain1;
    DX12_CHECK(Dx12Context.factory->CreateSwapChainForHwnd(
        Dx12Context.device->get_graphics_queue(), m_window_handle, &swapchain_desc, nullptr, nullptr, &swapchain1));

    swapchain1->QueryInterface(IID_PPV_ARGS(&m_swapchain));

    swapchain1->Release();
}

void Dx12Swapchain::retrieve_swapchain_images()
{
    MIZU_ASSERT(m_images.empty(), "Image vector should be empty");

    m_images.resize(SWAPCHAIN_BUFFER_COUNT);

    for (uint32_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; ++i)
    {
        ID3D12Resource* back_buffer;
        DX12_CHECK(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));

        const uint32_t width = static_cast<uint32_t>(back_buffer->GetDesc().Width);
        const uint32_t height = static_cast<uint32_t>(back_buffer->GetDesc().Height);

        m_images[i] = std::make_shared<Dx12ImageResource>(width, height, SWAPCHAIN_FORMAT, back_buffer, false);
    }
}

} // namespace Mizu::Dx12