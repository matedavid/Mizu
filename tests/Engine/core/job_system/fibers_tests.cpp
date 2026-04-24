#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/job_system/fibers.h"

using namespace Mizu;

uintptr_t align_down(uintptr_t p, size_t a)
{
    return p & ~(uintptr_t)(a - 1);
}

void fiber_entry_noop(void*) {}

struct FiberSwitchTestState
{
    FiberHandle* main_fiber = nullptr;
    FiberHandle* child_fiber = nullptr;
    int step = 0;
};

void fiber_switch_test_entry(void* arg)
{
    auto* state = static_cast<FiberSwitchTestState*>(arg);

    state->step = 1;
    fiber_switch(*state->child_fiber, *state->main_fiber);

    state->step = 2;
    fiber_switch(*state->child_fiber, *state->main_fiber);
}

TEST_CASE("FiberContext keeps expected ABI layout invariants", "[Fibers]")
{
    REQUIRE(alignof(FiberContext) == 16);
    REQUIRE((sizeof(FiberContext) % 16) == 0);
    REQUIRE((offsetof(FiberContext, rsp) % 8) == 0);
    REQUIRE((offsetof(FiberContext, rip) % 8) == 0);
}

TEST_CASE("fiber_create initializes context with aligned stack and valid entry point", "[Fibers]")
{
    alignas(16) std::array<uint8_t, 512> stack{};
    void* argument = reinterpret_cast<void*>(uintptr_t{0x1234});

    const FiberHandle handle = fiber_create(stack.data(), stack.size(), &fiber_entry_noop, argument);

    REQUIRE(handle.stack_ptr == stack.data());
    REQUIRE(handle.context.rip == reinterpret_cast<uintptr_t>(&fiber_entry_noop));

    const uintptr_t stack_top = align_down(reinterpret_cast<uintptr_t>(stack.data()) + stack.size(), 16);
#if MIZU_PLATFORM_WINDOWS
    const uintptr_t expected_rsp = stack_top - sizeof(uintptr_t) - uintptr_t{32};
#else
    const uintptr_t expected_rsp = stack_top - sizeof(uintptr_t);
#endif
    REQUIRE(handle.context.rsp == expected_rsp);
    REQUIRE(((handle.context.rsp + sizeof(uintptr_t)) % 16) == 0);

#if MIZU_PLATFORM_WINDOWS
    REQUIRE(handle.context.rcx == reinterpret_cast<uintptr_t>(argument));
#elif MIZU_PLATFORM_UNIX
    REQUIRE(handle.context.rdi == reinterpret_cast<uintptr_t>(argument));
#endif
}

TEST_CASE("fiber_convert_thread_to_fiber currently returns default-initialized handle", "[Fibers]")
{
    const FiberHandle handle = fiber_convert_thread_to_fiber();

    REQUIRE(handle.stack_ptr == nullptr);
    REQUIRE(handle.context.rsp == 0);
    REQUIRE(handle.context.rip == 0);
}

TEST_CASE("fiber_revert_fiber_to_thread is currently a no-op", "[Fibers]")
{
    const FiberHandle handle = fiber_convert_thread_to_fiber();

    fiber_revert_fiber_to_thread(handle);

    SUCCEED();
}

TEST_CASE("fiber_switch transfers control to a child fiber and resumes", "[Fibers]")
{
    alignas(16) std::array<uint8_t, 4096> stack{};

    FiberHandle main_fiber = fiber_convert_thread_to_fiber();
    FiberHandle child_fiber{};

    FiberSwitchTestState state{};
    state.main_fiber = &main_fiber;
    state.child_fiber = &child_fiber;

    child_fiber = fiber_create(stack.data(), stack.size(), &fiber_switch_test_entry, &state);

    REQUIRE(child_fiber.stack_ptr == stack.data());
    REQUIRE(state.step == 0);

    fiber_switch(main_fiber, child_fiber);
    REQUIRE(state.step == 1);

    fiber_switch(main_fiber, child_fiber);
    REQUIRE(state.step == 2);
}

TEST_CASE("fiber_switch supports independent child fibers", "[Fibers]")
{
    alignas(16) std::array<uint8_t, 4096> stack_a{};
    alignas(16) std::array<uint8_t, 4096> stack_b{};

    FiberHandle main_fiber = fiber_convert_thread_to_fiber();
    FiberHandle child_a{};
    FiberHandle child_b{};

    FiberSwitchTestState state_a{};
    state_a.main_fiber = &main_fiber;
    state_a.child_fiber = &child_a;

    FiberSwitchTestState state_b{};
    state_b.main_fiber = &main_fiber;
    state_b.child_fiber = &child_b;

    child_a = fiber_create(stack_a.data(), stack_a.size(), &fiber_switch_test_entry, &state_a);
    child_b = fiber_create(stack_b.data(), stack_b.size(), &fiber_switch_test_entry, &state_b);

    fiber_switch(main_fiber, child_a);
    REQUIRE(state_a.step == 1);

    fiber_switch(main_fiber, child_b);
    REQUIRE(state_b.step == 1);

    fiber_switch(main_fiber, child_a);
    REQUIRE(state_a.step == 2);

    fiber_switch(main_fiber, child_b);
    REQUIRE(state_b.step == 2);
}
