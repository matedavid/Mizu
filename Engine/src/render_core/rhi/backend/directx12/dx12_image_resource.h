#pragma once

#include "render_core/rhi/image_resource.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

class Dx12ImageResource
{
  public:
    static DXGI_FORMAT get_image_format(ImageFormat format);
};

} // namespace Mizu::Dx12
