#pragma once

#define MIZU_DX12_VALIDATIONS_ENABLED MIZU_DEBUG

#include <d3d12.h>
#include <dxcore.h>
#include <dxgi1_6.h>

#if MIZU_DX12_VALIDATIONS_ENABLED
#include <dxgidebug.h>
#endif

// Undefine near and far macro definitions from Windows
#undef near
#undef far

#include "base/debug/assert.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

#define DX12_CHECK_RESULT(expression) SUCCEEDED((expression))

#define DX12_CHECK(expression)                                                    \
    do                                                                            \
    {                                                                             \
        const HRESULT _dx12_check_res = expression;                               \
        MIZU_ASSERT(DX12_CHECK_RESULT(_dx12_check_res), "DirectX12 call failed"); \
    } while (false);

#else

#define DX12_CHECK_RESULT(expression) (expression)

#define DX12_CHECK(expression) expression

#endif

} // namespace Mizu::Dx12
