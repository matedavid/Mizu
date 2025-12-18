#include "render_core/rhi/swapchain.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Swapchain> Swapchain::create(const SwapchainDescription& desc)
{
    (void)desc;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu