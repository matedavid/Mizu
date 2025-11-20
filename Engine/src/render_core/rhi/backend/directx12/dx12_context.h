#pragma once

#include <memory>
#include <string_view>

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/backend/directx12/dx12_device.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

class Dx12Debug
{
  public:
    static void set_resource_name(ID3D12Resource* resource, std::string_view name);
};

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name) Dx12Debug::set_resource_name(resource, name)

#else

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name)

#endif

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