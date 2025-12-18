#include "render_core/rhi/device.h"

#include <windows.h>

#include "base/debug/assert.h"

namespace Mizu
{

std::shared_ptr<Device> Device::create(const DeviceCreationDescription& desc)
{
    const char* dll_path = nullptr;

    switch (desc.api)
    {
    case GraphicsApi2::Dx12:
        dll_path = nullptr;
        break;
    case GraphicsApi2::Vulkan:
        dll_path = MIZU_RENDER_CORE_VULKAN_DLL_PATH;
        break;
    }

    MIZU_VERIFY(dll_path != nullptr, "Did not select library path");

    HMODULE library = LoadLibraryA(dll_path);
    MIZU_ASSERT(library != nullptr, "Failed to load RenderCore library with path: {}", dll_path);

    create_rhi_device_func create_func = (create_rhi_device_func)GetProcAddress(library, "create_rhi_device");
    MIZU_ASSERT(create_func, "Failed to find create_rhi_device function on library with path: {}", dll_path);

    Device* device = create_func(desc);
    return std::shared_ptr<Device>(device);
}

} // namespace Mizu