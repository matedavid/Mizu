#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Device
{
  public:
    Dx12Device();
    ~Dx12Device();

    ID3D12Device* handle() const { return m_device; }

  private:
    ID3D12Device* m_device = nullptr;
    IDXGIAdapter1* m_adapter = nullptr;
};

} // namespace Mizu::Dx12