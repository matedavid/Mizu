#pragma once

#include <cstdint>
#include <stddef.h>
#if MIZU_PLATFORM_WINDOWS
#include <xmmintrin.h>
#endif

#include "mizu_core_module.h"

namespace Mizu
{

#if MIZU_PLATFORM_WINDOWS

struct alignas(16) FiberSimdState
{
    __m128 xmm6{};
    __m128 xmm7{};
    __m128 xmm8{};
    __m128 xmm9{};
    __m128 xmm10{};
    __m128 xmm11{};
    __m128 xmm12{};
    __m128 xmm13{};
    __m128 xmm14{};
    __m128 xmm15{};
};

#endif

struct alignas(16) FiberContext
{
    // calle-saved registers (non-volatile)

    // general purpose registers
    uintptr_t rbx = 0;
    uintptr_t rbp = 0;
    uintptr_t r12 = 0;
    uintptr_t r13 = 0;
    uintptr_t r14 = 0;
    uintptr_t r15 = 0;

#if MIZU_PLATFORM_WINDOWS
    // preserved general purpose registers
    uintptr_t rdi = 0;
    uintptr_t rsi = 0;
    // bootstrap argument register for first entry on Windows x64
    uintptr_t rcx = 0;
#endif

#if MIZU_PLATFORM_UNIX
    uintptr_t rdi = 0;
#endif

    // stack pointer and instruction pointer
    uintptr_t rsp = 0;
    uintptr_t rip = 0;

#if MIZU_PLATFORM_WINDOWS
    FiberSimdState simd_state{};
#endif
};

static_assert(alignof(FiberContext) == 16);
static_assert(sizeof(FiberContext) % 16 == 0);

static_assert(offsetof(FiberContext, rsp) % 8 == 0);
static_assert(offsetof(FiberContext, rip) % 8 == 0);

struct FiberHandle
{
    void* stack_ptr = nullptr;
    FiberContext context{};
};

using FiberStartFunc = void (*)(void*);

MIZU_CORE_API FiberHandle fiber_convert_thread_to_fiber();
MIZU_CORE_API void fiber_revert_fiber_to_thread(const FiberHandle& handle);

MIZU_CORE_API FiberHandle
fiber_create(uint8_t* stack_memory, size_t stack_size, FiberStartFunc func, void* arg = nullptr);
MIZU_CORE_API void fiber_destroy(const FiberHandle& handle);

MIZU_CORE_API void fiber_switch(FiberHandle& source, const FiberHandle& dest);

} // namespace Mizu