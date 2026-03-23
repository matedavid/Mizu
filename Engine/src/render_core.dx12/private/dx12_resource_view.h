#pragma once

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
    uint32_t stride;
    ResourceViewType type;
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
};

struct Dx12ImageResourceView
{
    ImageResourceViewDescription description;
    ImageFormat format;
    ResourceViewType type;
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
};

void create_buffer_srv(
    const Dx12BufferResource& resource,
    const BufferResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle);
void create_buffer_uav(
    const Dx12BufferResource& resource,
    const BufferResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle);
void create_buffer_cbv(
    const Dx12BufferResource& resource,
    const BufferResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle);

void create_image_srv(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle);
void create_image_uav(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle);
void create_image_rtv(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle);
void create_image_dsv(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle);

} // namespace Mizu::Dx12
