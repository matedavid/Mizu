#include "render_core/rhi/device.h"

// TODO: we should abstract this into a platform layer to correctly use the LoadLibraryA on a platform basis
#if MIZU_PLATFORM_WINDOWS

#include <windows.h>

// Undefine near and far macro definitions from Windows
#undef near
#undef far

#define LIBRARY_HANDLE HMODULE
#define LOAD_LIBRARY(_path) LoadLibraryA(_path)
#define FREE_LIBRARY(_library) FreeLibrary(_library)
#define GET_PROC_ADDRESS(_library, _func_name) GetProcAddress(_library, _func_name)

#elif MIZU_PLATFORM_UNIX

#include <dlfcn.h>

#define LIBRARY_HANDLE void*
#define LOAD_LIBRARY(_path) dlopen(_path, RTLD_NOW)
#define FREE_LIBRARY(_library) dlclose(_library)
#define GET_PROC_ADDRESS(_library, _func_name) dlsym(_library, _func_name)

#endif

#include "base/debug/assert.h"

#include "render_core/allocators/gpu_block_allocator.h"

namespace Mizu
{

static LIBRARY_HANDLE s_library_handle = nullptr;

Device* Device::create(const DeviceCreationDescription& desc)
{
    MIZU_ASSERT(s_library_handle == nullptr, "Device already created");

    const char* dll_path = nullptr;

    switch (desc.api)
    {
    case GraphicsApi::Dx12:
#ifdef MIZU_RENDER_CORE_DX12_DLL_PATH
        dll_path = MIZU_RENDER_CORE_DX12_DLL_PATH;
#else
        MIZU_UNREACHABLE("DirectX12 RenderCore backend was requested but was not built in this configuration");
#endif
        break;
    case GraphicsApi::Vulkan:
#ifdef MIZU_RENDER_CORE_VULKAN_DLL_PATH
        dll_path = MIZU_RENDER_CORE_VULKAN_DLL_PATH;
#else
        MIZU_UNREACHABLE("Vulkan RenderCore backend was requested but was not built in this configuration");
#endif
        break;
    }

    MIZU_VERIFY(dll_path != nullptr, "Did not select library path");

    s_library_handle = LOAD_LIBRARY(dll_path);
    MIZU_ASSERT(s_library_handle != nullptr, "Failed to load RenderCore library with path: {}", dll_path);

    create_rhi_device_func create_func = reinterpret_cast<create_rhi_device_func>(
        reinterpret_cast<void*>(GET_PROC_ADDRESS(s_library_handle, "create_rhi_device")));
    MIZU_ASSERT(create_func, "Failed to find create_rhi_device function on library with path: {}", dll_path);

    Device* device = create_func(desc);
    MIZU_ASSERT(device != nullptr, "Failed to create Rhi Device");

    GpuBlockAllocatorDescription default_allocator_desc{};
    default_allocator_desc.name = "DefaultGpuAllocator";
    device->m_default_allocator = std::make_unique<GpuBlockAllocator>(*device, default_allocator_desc);

    return device;
}

void Device::free()
{
    if (s_library_handle)
    {
        FREE_LIBRARY(s_library_handle);
        s_library_handle = nullptr;
    }
}

// Needed here so that we can instantiate m_default_allocator without including the types' header on the .h
Device::Device() = default;

Device::~Device()
{
    MIZU_ASSERT(
        m_default_allocator == nullptr,
        "The backend device must call destroy_default_allocator() before destroying the native device");
}

std::shared_ptr<BufferResource> Device::create_buffer(const BufferDescription& desc, HeapDomain domain) const
{
    return get_default_allocator().create_buffer(desc, domain);
}

std::shared_ptr<ImageResource> Device::create_image(const ImageDescription& desc, HeapDomain domain) const
{
    return get_default_allocator().create_image(desc, domain);
}

GpuAllocator& Device::get_default_allocator() const
{
    MIZU_ASSERT(m_default_allocator != nullptr, "Default allocator is not available");
    return *m_default_allocator;
}

void Device::destroy_default_allocator()
{
    m_default_allocator.reset();
    m_default_allocator = nullptr;
}

std::shared_ptr<DescriptorSet> Device::allocate_descriptor_set(
    std::span<const DescriptorItem> layout,
    DescriptorSetAllocationType type,
    uint32_t variable_count) const
{
    DescriptorSetLayoutDescription layout_desc{};
    layout_desc.layout = layout;
    layout_desc.vulkan_apply_binding_offsets = true;

    const DescriptorSetLayoutHandle handle = create_descriptor_set_layout(layout_desc);
    return allocate_descriptor_set(handle, type, variable_count);
}

} // namespace Mizu
