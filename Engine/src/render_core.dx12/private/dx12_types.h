#pragma once

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/render_pass.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

D3D12_RESOURCE_FLAGS get_dx12_buffer_usage(BufferUsageBits usage);
D3D12_RESOURCE_STATES get_dx12_buffer_resource_state(BufferResourceState state);

DXGI_FORMAT get_dx12_image_format(ImageFormat format);
D3D12_RESOURCE_DIMENSION get_dx12_image_type(ImageType type);
D3D12_RESOURCE_FLAGS get_dx12_usage(ImageUsageBits usage, ImageFormat format);
D3D12_RESOURCE_STATES get_dx12_image_resource_state(ImageResourceState state);
D3D12_BARRIER_LAYOUT get_dx12_image_barrier_layout(ImageResourceState state);

D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE get_dx12_load_operation(LoadOperation operation);
D3D12_RENDER_PASS_ENDING_ACCESS_TYPE get_dx12_store_operation(StoreOperation operation);

} // namespace Mizu::Dx12
