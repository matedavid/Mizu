#include "dx12_context.h"

#if defined(MIZU_RENDER_CORE_DX12_AGILITY_SDK_VERSION) && defined(MIZU_RENDER_CORE_DX12_AGILITY_SDK_PATH)

extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = MIZU_RENDER_CORE_DX12_AGILITY_SDK_VERSION;
}

extern "C"
{
    __declspec(dllexport) extern const char* D3D12SDKPath = MIZU_RENDER_CORE_DX12_AGILITY_SDK_PATH;
}

#endif

namespace Mizu::Dx12
{

Dx12ContextT Dx12Context{};

Dx12ContextT::~Dx12ContextT() = default;

} // namespace Mizu::Dx12