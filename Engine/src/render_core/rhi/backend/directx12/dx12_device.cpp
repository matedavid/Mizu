#include "dx12_device.h"

#include "base/debug/logging.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"

namespace Mizu::Dx12
{

Dx12Device::Dx12Device()
{
    IDXCoreAdapterFactory* adapter_factory;
    DX12_CHECK(DXCoreCreateAdapterFactory(&adapter_factory));

    constexpr uint32_t DISCRETE_GPU_SCORE = 40;

    IDXGIAdapter1* best_adapter = nullptr;
    uint32_t best_adapter_score = 0;

    IDXGIAdapter1* tmp_adapter;
    for (uint32_t i = 0; Dx12Context.factory->EnumAdapters1(i, &tmp_adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        IDXCoreAdapter* core_adapter;

        DXGI_ADAPTER_DESC1 desc;
        tmp_adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select software adapter
            continue;
        }

        // Check to see if the adapter supports Direct3D 12
        if (!DX12_CHECK_RESULT(D3D12CreateDevice(tmp_adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }

        uint32_t adapter_score = 0;

        DX12_CHECK(adapter_factory->GetAdapterByLuid(desc.AdapterLuid, &core_adapter));

        bool is_dedicated_gpu = false;
        DX12_CHECK(core_adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &is_dedicated_gpu));

        if (is_dedicated_gpu)
        {
            adapter_score += DISCRETE_GPU_SCORE;
        }

        if (adapter_score > best_adapter_score)
        {
            if (best_adapter != nullptr)
                best_adapter->Release();

            best_adapter_score = adapter_score;
            best_adapter = tmp_adapter;
        }
        else
        {
            tmp_adapter->Release();
        }
    }

    MIZU_VERIFY(best_adapter != nullptr, "Failed to find suitable adapter");

    m_adapter = best_adapter;
    DX12_CHECK_RESULT(D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));

#if MIZU_DEBUG
    DXGI_ADAPTER_DESC1 adapter_desc;
    m_adapter->GetDesc1(&adapter_desc);

    std::wstring wname(adapter_desc.Description);
    const int32_t size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);

    std::string name(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &name[0], size_needed, nullptr, nullptr);

    MIZU_LOG_INFO("Selected device: {}", name);
#endif
}

Dx12Device::~Dx12Device()
{
    m_device->Release();
    m_adapter->Release();
}

} // namespace Mizu::Dx12