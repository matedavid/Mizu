#include "dx12_types.h"

namespace Mizu::Dx12
{

D3D12_RESOURCE_FLAGS get_dx12_buffer_usage(BufferUsageBits usage)
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    if (usage & BufferUsageBits::UnorderedAccess)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    return flags;
}

D3D12_RESOURCE_STATES get_dx12_buffer_resource_state(BufferResourceState state)
{
    switch (state)
    {
    case BufferResourceState::Undefined:
        return D3D12_RESOURCE_STATE_COMMON;
    case BufferResourceState::ShaderReadOnly:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
               | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    case BufferResourceState::UnorderedAccess:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case BufferResourceState::TransferSrc:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case BufferResourceState::TransferDst:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case BufferResourceState::AccelStructScratch:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case BufferResourceState::AccelStructBuildInput:
        return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
}

DXGI_FORMAT get_dx12_image_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::R16G16_SFLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case ImageFormat::R32G32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::R32G32B32_SFLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case ImageFormat::R8G8B8A8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case ImageFormat::R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::R16G16B16A16_SFLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case ImageFormat::R32G32B32A32_SFLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case ImageFormat::B8G8R8A8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case ImageFormat::B8G8R8A8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case ImageFormat::D32_SFLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    }
}

D3D12_RESOURCE_DIMENSION get_dx12_image_type(ImageType type)
{
    switch (type)
    {
    case ImageType::Image1D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    case ImageType::Image2D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    case ImageType::Image3D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    case ImageType::Cubemap:
        MIZU_UNREACHABLE("Not implemented");
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }

    MIZU_UNREACHABLE("Invalid ImageType");
}

D3D12_RESOURCE_FLAGS get_dx12_usage(ImageUsageBits usage, ImageFormat format)
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    if (usage & ImageUsageBits::Attachment && is_depth_format(format))
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    else if (usage & ImageUsageBits::Attachment)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    if (usage & ImageUsageBits::UnorderedAccess)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    return flags;
}

D3D12_RESOURCE_STATES get_dx12_image_resource_state(ImageResourceState state)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return D3D12_RESOURCE_STATE_COMMON;
    case ImageResourceState::ShaderReadOnly:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case ImageResourceState::UnorderedAccess:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case ImageResourceState::TransferSrc:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case ImageResourceState::TransferDst:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case ImageResourceState::ColorAttachment:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case ImageResourceState::DepthStencilAttachment:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case ImageResourceState::Present:
        return D3D12_RESOURCE_STATE_PRESENT;
    }
}

D3D12_BARRIER_LAYOUT get_dx12_image_barrier_layout(ImageResourceState state, ImageFormat format)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return D3D12_BARRIER_LAYOUT_UNDEFINED;
    case ImageResourceState::ShaderReadOnly: {
        if (is_depth_format(format))
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        else
            return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
    }
    case ImageResourceState::UnorderedAccess:
        return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    case ImageResourceState::TransferSrc:
        return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
    case ImageResourceState::TransferDst:
        return D3D12_BARRIER_LAYOUT_COPY_DEST;
    case ImageResourceState::ColorAttachment:
        return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    case ImageResourceState::DepthStencilAttachment:
        return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
    case ImageResourceState::Present:
        return D3D12_BARRIER_LAYOUT_PRESENT;
    }
}

D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE get_dx12_load_operation(LoadOperation operation)
{
    switch (operation)
    {
    case LoadOperation::Load:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    case LoadOperation::Clear:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
    case LoadOperation::DontCare:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
    }
}

D3D12_RENDER_PASS_ENDING_ACCESS_TYPE get_dx12_store_operation(StoreOperation operation)
{
    switch (operation)
    {
    case StoreOperation::Store:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    case StoreOperation::DontCare:
    case StoreOperation::None:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
    }
}

} // namespace Mizu::Dx12
