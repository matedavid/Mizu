#include "render_core/rhi/synchronization.h"

#include "backend/dx12/dx12_synchronization.h"
#include "backend/vulkan/vulkan_synchronization.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Fence> Fence::create()
{
    return Fence::create(true);
}

std::shared_ptr<Fence> Fence::create(bool signaled)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Fence>(signaled);
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanFence>(signaled);
    }
}

std::shared_ptr<Semaphore> Semaphore::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Semaphore>();
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanSemaphore>();
    }
}

} // namespace Mizu
