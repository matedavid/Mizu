#include "render_core/rhi/device_memory_allocator.h"

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

//
// BaseDeviceMemoryAllocator
//

std::shared_ptr<BaseDeviceMemoryAllocator> BaseDeviceMemoryAllocator::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

//
// AliasedDeviceMemoryAllocator
//

std::shared_ptr<AliasedDeviceMemoryAllocator> AliasedDeviceMemoryAllocator::create(bool host_visible, std::string name)
{
    (void)host_visible;
    (void)name;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu