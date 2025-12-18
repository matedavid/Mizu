#include "render_core/rhi/framebuffer.h"

#include "backend/dx12/dx12_framebuffer.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Framebuffer> Framebuffer::create(const FramebufferDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Framebuffer>(desc);
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu
