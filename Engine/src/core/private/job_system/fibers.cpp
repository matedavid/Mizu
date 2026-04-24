#include "core/job_system/fibers.h"

#include "base/debug/assert.h"

namespace Mizu
{

static uintptr_t align_down(uintptr_t p, size_t a)
{
    return p & ~(uintptr_t)(a - 1);
}

FiberHandle fiber_convert_thread_to_fiber()
{
    FiberHandle handle{};
    handle.stack_ptr = nullptr;
    handle.context = FiberContext{};

    return handle;
}

void fiber_revert_fiber_to_thread([[maybe_unused]] const FiberHandle& handle)
{
    // Empty implementation
}

static constexpr size_t FiberStackAlignment = 16;
#if MIZU_PLATFORM_WINDOWS
static constexpr size_t FiberWindowsShadowSpace = 32;
#endif

static bool fiber_fill_context(
    FiberContext& context,
    uint8_t* stack_memory,
    size_t stack_size,
    FiberStartFunc func,
    void* arg)
{
    if (func == nullptr)
    {
        MIZU_ASSERT(false, "Fiber start function cannot be null");
        return false;
    }

    if (stack_memory == nullptr)
    {
        MIZU_ASSERT(false, "Fiber stack memory cannot be null");
        return false;
    }

    uintptr_t stack_base = reinterpret_cast<uintptr_t>(stack_memory) + stack_size;
    stack_base = align_down(stack_base, FiberStackAlignment);

    // Reserve call frame for first fiber entry:
    // [rsp + 0]   = return address
    // [rsp + 8..] = Windows x64 shadow space for callees
#if MIZU_PLATFORM_WINDOWS
    stack_base -= sizeof(uintptr_t) + FiberWindowsShadowSpace;
#elif MIZU_PLATFORM_UNIX
    stack_base -= sizeof(uintptr_t);
#endif

    // Set fake return address (fiber_exit or crash handler)
    //*reinterpret_cast<uintptr_t*>(stack_base) = reinterpret_cast<uintptr_t>(fiber_exit);

    context.rip = reinterpret_cast<uintptr_t>(func);
    context.rsp = stack_base;
#if MIZU_PLATFORM_WINDOWS
    context.rcx = reinterpret_cast<uintptr_t>(arg);
#elif MIZU_PLATFORM_UNIX
    context.rdi = reinterpret_cast<uintptr_t>(arg);
#endif

    return true;
}

FiberHandle fiber_create(uint8_t* stack_memory, size_t stack_size, FiberStartFunc func, void* arg)
{
    const uintptr_t uintptr_stack = reinterpret_cast<uintptr_t>(stack_memory);
    if ((uintptr_stack & (FiberStackAlignment - 1)) != 0)
    {
        MIZU_ASSERT(false, "Fiber stack memory must be aligned to {} bytes", FiberStackAlignment);
        return FiberHandle{};
    }

    FiberHandle handle{};
    handle.stack_ptr = stack_memory;
    handle.context = FiberContext{};

    if (!fiber_fill_context(handle.context, stack_memory, stack_size, func, arg))
    {
        return FiberHandle{};
    }

    return handle;
}

void fiber_destroy(const FiberHandle& handle)
{
    (void)handle;
}

extern "C" void fiber_switch_internal(FiberContext& from, const FiberContext& to);

void fiber_switch(FiberHandle& source, const FiberHandle& dest)
{
    fiber_switch_internal(source.context, dest.context);
}

} // namespace Mizu