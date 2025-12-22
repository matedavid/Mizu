#pragma once

#include <memory>

#include "render_core/rhi/resource_view.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

// Forward declarations
class Dx12BufferResource;
class Dx12ImageResource;

struct Dx12BufferResourceView
{
    uint64_t offset, size;
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
};

struct Dx12ImageResourceView
{
    ImageResourceViewDescription description;
    ImageFormat format;
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
};

D3D12_CPU_DESCRIPTOR_HANDLE create_buffer_cpu_descriptor_handle(
    const Dx12BufferResource& resource,
    ResourceViewType type);
void free_buffer_cpu_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle);

D3D12_CPU_DESCRIPTOR_HANDLE create_image_cpu_descriptor_handle(
    const ImageResourceViewDescription& desc,
    const Dx12ImageResource& resource,
    ResourceViewType type);

void free_image_cpu_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle, ResourceViewType type, ImageFormat format);

} // namespace Mizu::Dx12