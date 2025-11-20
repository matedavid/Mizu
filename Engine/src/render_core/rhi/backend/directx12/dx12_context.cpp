#include "dx12_context.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

const size_t WIDECHAR_NAME_DEFULT_SIZE = 128;

void Dx12Debug::set_resource_name(ID3D12Resource* resource, std::string_view name)
{
    wchar_t widechar_name[WIDECHAR_NAME_DEFULT_SIZE];
    MultiByteToWideChar(CP_UTF8, 0, name.data(), -1, widechar_name, WIDECHAR_NAME_DEFULT_SIZE);

    DX12_CHECK(resource->SetName(widechar_name));
}

#endif

Dx12ContextT Dx12Context{};

Dx12ContextT::~Dx12ContextT() = default;

} // namespace Mizu::Dx12