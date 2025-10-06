#include "dx12_backend.h"

#include "base/debug/assert.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{
bool Dx12Backend::initialize(const RendererConfiguration& config)
{
    MIZU_ASSERT(
        std::holds_alternative<Dx12SpecificConfiguration>(config.backend_specific_config),
        "backend_specific_configuration is not Dx12SpecificConfiguration");

    // Create Factory
    uint32_t dxgi_factory_flags = 0;

#if MIZU_DX12_VALIDATIONS_ENABLED
    ID3D12Debug* debug_controller;
    DX12_CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)));

    DX12_CHECK(debug_controller->QueryInterface(IID_PPV_ARGS(&Dx12Context.debug_controller)));
    Dx12Context.debug_controller->EnableDebugLayer();
    Dx12Context.debug_controller->SetEnableGPUBasedValidation(true);

    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;

    debug_controller->Release();
    debug_controller = nullptr;
#endif

    DX12_CHECK(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&Dx12Context.factory)));

    Dx12Context.device = std::make_unique<Dx12Device>();

#if MIZU_DX12_VALIDATIONS_ENABLED
    DX12_CHECK(Dx12Context.device->handle()->QueryInterface(&Dx12Context.debug_device));
#endif

    return true;
}

Dx12Backend::~Dx12Backend()
{
    // NOTE: Order of destruction matters

    Dx12Context.device.reset();

#if MIZU_DX12_VALIDATIONS_ENABLED
    Dx12Context.debug_controller->Release();
    Dx12Context.debug_device->Release();
#endif

    Dx12Context.factory->Release();
}

void Dx12Backend::wait_idle() const {}

RendererCapabilities Dx12Backend::get_capabilities() const
{
    return Dx12Context.device->get_device_capabilities();
}

} // namespace Mizu::Dx12
