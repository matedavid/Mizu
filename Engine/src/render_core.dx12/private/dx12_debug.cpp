#include "dx12_debug.h"

#if MIZU_DX12_VALIDATIONS_ENABLED

// pix3.h pulls additional Win32 headers that still rely on near/far being defined.
#define near
#define far

#include <pix3.h>

#undef near
#undef far

namespace Mizu::Dx12
{

static constexpr size_t WIDECHAR_NAME_DEFULT_SIZE = 128;

static void to_widechar_buffer(std::string_view text, wchar_t* buffer, size_t buffer_size)
{
    const int32_t max_chars = static_cast<int32_t>(buffer_size - 1);
    const int32_t source_size = static_cast<int32_t>(std::min(text.size(), static_cast<size_t>(max_chars)));
    const int32_t converted_chars = MultiByteToWideChar(CP_UTF8, 0, text.data(), source_size, buffer, max_chars);
    buffer[converted_chars > 0 ? converted_chars : 0] = L'\0';

    // PIX treats the incoming string as a format string, so sanitize '%' to avoid decoder bugs in capture tools.
    for (wchar_t* cursor = buffer; *cursor != L'\0'; ++cursor)
    {
        if (*cursor == L'%')
            *cursor = L'_';
    }
}

void Dx12Debug::begin_gpu_marker(ID3D12GraphicsCommandList* command_list, std::string_view label)
{
    wchar_t buffer[WIDECHAR_NAME_DEFULT_SIZE];
    to_widechar_buffer(label, buffer, WIDECHAR_NAME_DEFULT_SIZE);

    PIXBeginEvent(command_list, PIX_COLOR_INDEX(0), buffer);
}

void Dx12Debug::end_gpu_marker(ID3D12GraphicsCommandList* command_list)
{
    PIXEndEvent(command_list);
}

void Dx12Debug::set_resource_name(ID3D12Resource* resource, std::string_view name)
{
    wchar_t buffer[WIDECHAR_NAME_DEFULT_SIZE];
    to_widechar_buffer(name, buffer, WIDECHAR_NAME_DEFULT_SIZE);

    DX12_CHECK(resource->SetName(buffer));
}

} // namespace Mizu::Dx12

#endif
