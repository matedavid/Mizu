#include "render_core/rhi/device.h"

#include <windows.h>

#include "base/debug/assert.h"

namespace Mizu
{

Device* Device::create(const DeviceCreationDescription& desc)
{
    const char* dll_path = nullptr;

    switch (desc.api)
    {
    case GraphicsApi::Dx12:
        dll_path = MIZU_RENDER_CORE_DX12_DLL_PATH;
        break;
    case GraphicsApi::Vulkan:
        dll_path = MIZU_RENDER_CORE_VULKAN_DLL_PATH;
        break;
    }

    MIZU_VERIFY(dll_path != nullptr, "Did not select library path");

    HMODULE library = LoadLibraryA(dll_path);
    MIZU_ASSERT(library != nullptr, "Failed to load RenderCore library with path: {}", dll_path);

    create_rhi_device_func create_func =
        reinterpret_cast<create_rhi_device_func>(reinterpret_cast<void*>(GetProcAddress(library, "create_rhi_device")));
    MIZU_ASSERT(create_func, "Failed to find create_rhi_device function on library with path: {}", dll_path);

    Device* device = create_func(desc);
    MIZU_ASSERT(device != nullptr, "Failed to create Rhi Device");

    return device;
}

} // namespace Mizu