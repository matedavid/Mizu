#include "dx12_image_resource.h"

namespace Mizu::Dx12
{

DXGI_FORMAT Dx12ImageResource::get_image_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::RG16_SFLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case ImageFormat::RG32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::RGB32_SFLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case ImageFormat::RGBA8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case ImageFormat::RGBA8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RGBA16_SFLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case ImageFormat::RGBA32_SFLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case ImageFormat::BGRA8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case ImageFormat::D32_SFLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    }
}

} // namespace Mizu::Dx12