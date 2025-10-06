#pragma once

#include <memory>

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/backend/directx12/dx12_device.h"

namespace Mizu::Dx12
{

struct Dx12ContextT
{
    ~Dx12ContextT();

    IDXGIFactory4* factory;

#if MIZU_DX12_VALIDATIONS_ENABLED
    ID3D12Debug1* debug_controller;
    ID3D12DebugDevice* debug_device;
#endif

    std::unique_ptr<Dx12Device> device;
};

extern Dx12ContextT Dx12Context;

} // namespace Mizu::Dx12