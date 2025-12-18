#include "render_core/rhi/framebuffer.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Framebuffer> Framebuffer::create(const FramebufferDescription& desc)
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
