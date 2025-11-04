#include "device_memory_allocator.h"

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_device_memory_allocator.h"
#include "render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.h"

namespace Mizu
{

//
// BaseDeviceMemoryAllocator
//

std::shared_ptr<BaseDeviceMemoryAllocator> BaseDeviceMemoryAllocator::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12BaseDeviceMemoryAllocator>();
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBaseDeviceMemoryAllocator>();
    }
}

//
// AliasedDeviceMemoryAllocator
//

std::shared_ptr<AliasedDeviceMemoryAllocator> AliasedDeviceMemoryAllocator::create(bool host_visible, std::string name)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanAliasedDeviceMemoryAllocator>(host_visible, std::move(name));
    }
}

} // namespace Mizu