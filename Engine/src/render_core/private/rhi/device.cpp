#include "render_core/rhi/device.h"

#include <windows.h>

// Undefine near and far macro definitions from Windows
#undef near
#undef far

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

std::shared_ptr<DescriptorSet> Device::allocate_descriptor_set(
    std::span<const DescriptorItem> layout,
    DescriptorSetAllocationType type,
    uint32_t variable_count) const
{
    DescriptorSetLayoutDescription layout_desc{};
    layout_desc.layout = layout;
    layout_desc.vulkan_apply_binding_offsets = true;

    const PipelineLayoutHandle handle = create_descriptor_set_layout(layout_desc);
    return allocate_descriptor_set(handle, type, variable_count);
}

} // namespace Mizu