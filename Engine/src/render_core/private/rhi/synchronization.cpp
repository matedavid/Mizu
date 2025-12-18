#include "render_core/rhi/synchronization.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Fence> Fence::create()
{
    return Fence::create(true);
}

std::shared_ptr<Fence> Fence::create(bool signaled)
{
    (void)signaled;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

std::shared_ptr<Semaphore> Semaphore::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu
