#include "framebuffer.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"

namespace Mizu
{

std::shared_ptr<Framebuffer> Framebuffer::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Framebuffer>(desc);
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanFramebuffer>(desc);
    }
}

} // namespace Mizu
