#include "backend/dx12/dx12_context.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

static constexpr size_t WIDECHAR_NAME_DEFULT_SIZE = 128;
static wchar_t WIDECHAR_NAME_BUFFER[WIDECHAR_NAME_DEFULT_SIZE] = {0};

void Dx12Debug::set_resource_name(ID3D12Resource* resource, std::string_view name)
{
    MultiByteToWideChar(CP_UTF8, 0, name.data(), -1, WIDECHAR_NAME_BUFFER, WIDECHAR_NAME_DEFULT_SIZE);
    DX12_CHECK(resource->SetName(WIDECHAR_NAME_BUFFER));
}

#endif

Dx12ContextT Dx12Context{};

Dx12ContextT::~Dx12ContextT() = default;

} // namespace Mizu::Dx12