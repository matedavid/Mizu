#pragma once

// Prevent Windows libraries from defining min and max as macros
#define NOMINMAX

#include <d3d12.h>
#include <dxcore.h>
#include <dxgi1_6.h>

// Undefine near and far macro definitions from Windows
#undef near
#undef far

#include "base/debug/assert.h"

namespace Mizu::Dx12
{

#define MIZU_DX12_VALIDATIONS_ENABLED MIZU_DEBUG

#if MIZU_DEBUG

#define DX12_CHECK_RESULT(expression) SUCCEEDED((expression))

#define DX12_CHECK(expression)                                                    \
    do                                                                            \
    {                                                                             \
        const HRESULT _dx12_check_res = expression;                               \
        MIZU_ASSERT(DX12_CHECK_RESULT(_dx12_check_res), "DirectX12 call failed"); \
    } while (false);

#else

#define DX12_CHECK_RESULT(expression) (void)expression

#define DX12_CHECK(expression) expression

#endif

} // namespace Mizu::Dx12
