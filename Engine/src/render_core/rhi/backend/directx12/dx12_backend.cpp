#include "dx12_backend.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

static void d3d12_validation_message_callback(
    [[maybe_unused]] D3D12_MESSAGE_CATEGORY category,
    D3D12_MESSAGE_SEVERITY severity,
    [[maybe_unused]] D3D12_MESSAGE_ID id,
    [[maybe_unused]] LPCSTR pDescription,
    [[maybe_unused]] void* pContext)
{
    switch (severity)
    {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
    case D3D12_MESSAGE_SEVERITY_ERROR:
        MIZU_LOG_ERROR("[D3D12 Validation]: {}", pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_WARNING:
        MIZU_LOG_WARNING("[D3D12 Validation]: {}", pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_INFO:
    case D3D12_MESSAGE_SEVERITY_MESSAGE:
        MIZU_LOG_INFO("[D3D12 Validation]: {}", pDescription);
        break;
    }
}

#endif

bool Dx12Backend::initialize([[maybe_unused]] const RendererConfiguration& config)
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
    Dx12Context.debug_controller->SetEnableSynchronizedCommandQueueValidation(false);

    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;

    debug_controller->Release();
#endif

    DX12_CHECK(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&Dx12Context.factory)));

    Dx12Context.device = std::make_unique<Dx12Device>();

#if MIZU_DX12_VALIDATIONS_ENABLED
    DX12_CHECK(Dx12Context.device->handle()->QueryInterface(&Dx12Context.debug_device));

    Dx12Context.device->handle()->QueryInterface(IID_PPV_ARGS(&Dx12Context.debug_info_queue));
    DWORD cookie = 0;
    DX12_CHECK(Dx12Context.debug_info_queue->RegisterMessageCallback(
        d3d12_validation_message_callback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &cookie));
#endif

    Dx12Context.heaps.cbv_srv_uav_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        1000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    Dx12Context.heaps.rtv_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        100, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    Dx12Context.heaps.dsv_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        100, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    Dx12Context.heaps.sampler_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        100, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    Dx12Context.heaps.cbv_srv_uav_shader_heap =
        std::make_unique<Dx12DescriptorHeapGpuCircularBuffer>(100000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Dx12Context.heaps.sampler_shader_heap =
        std::make_unique<Dx12DescriptorHeapGpuCircularBuffer>(1000, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    return true;
}

Dx12Backend::~Dx12Backend()
{
    // NOTE: Order of destruction matters

    Dx12Context.heaps.sampler_shader_heap.reset();
    Dx12Context.heaps.cbv_srv_uav_shader_heap.reset();

    Dx12Context.heaps.sampler_heap.reset();
    Dx12Context.heaps.dsv_heap.reset();
    Dx12Context.heaps.rtv_heap.reset();
    Dx12Context.heaps.cbv_srv_uav_heap.reset();

    Dx12Context.device.reset();

#if MIZU_DX12_VALIDATIONS_ENABLED
    Dx12Context.debug_info_queue->Release();
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
