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

#if MIZU_DX12_VALIDATIONS_ENABLED
    IDXGIDebug1* dxgi_debug;
    if (DX12_CHECK_RESULT(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug))))
    {
        dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
    }

    dxgi_debug->Release();
#endif
}

void Dx12Backend::wait_idle() const
{
    ID3D12Fence* fence;
    DX12_CHECK(Dx12Context.device->handle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    constexpr uint64_t FENCE_VALUE = 1;
    DX12_CHECK(Dx12Context.device->get_graphics_queue()->Signal(fence, FENCE_VALUE));

    HANDLE event_handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (fence->GetCompletedValue() < FENCE_VALUE)
    {
        DX12_CHECK(fence->SetEventOnCompletion(FENCE_VALUE, event_handle));
        WaitForSingleObject(event_handle, INFINITE);
    }

    CloseHandle(event_handle);
    fence->Release();
}

RendererCapabilities Dx12Backend::get_capabilities() const
{
    return Dx12Context.device->get_device_capabilities();
}

} // namespace Mizu::Dx12
