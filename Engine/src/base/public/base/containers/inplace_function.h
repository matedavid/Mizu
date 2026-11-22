#pragma once

#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

#include "base/debug/assert.h"

namespace Mizu
{

// clang-format off
template <typename T, size_t CapacityBytes>
concept InplaceFunctionStorable = std::is_same_v<T, std::decay_t<T>>
                               && std::is_invocable_v<T&>
                               && std::is_move_constructible_v<T>
                               && sizeof(T) <= CapacityBytes
                               && alignof(T) <= alignof(std::max_align_t);
// clang-format on

template <size_t CapacityBytes>
class inplace_function
{
    static_assert(CapacityBytes > 0, "Can't create inplace_function with CapacityBytes == 0");

  public:
    inplace_function() = default;
    inplace_function(inplace_function&& other) { move_from(std::move(other)); }

    inplace_function& operator=(inplace_function&& other)
    {
        if (this != &other)
        {
            reset();
            move_from(std::move(other));
        }

        return *this;
    }

    inplace_function(const inplace_function&) = delete;
    inplace_function& operator=(const inplace_function&) = delete;

    ~inplace_function() { reset(); }

    template <typename Func>
        requires InplaceFunctionStorable<std::decay_t<Func>, CapacityBytes>
    static inplace_function create(Func&& func)
    {
        using StoredFuncT = std::decay_t<Func>;

        inplace_function result{};
        new (&result.m_memory) StoredFuncT(std::forward<Func>(func));

        result.m_invoke_func = [](void* memory) {
            StoredFuncT& func = *std::launder(reinterpret_cast<StoredFuncT*>(memory));
            std::invoke(func);
        };

        result.m_destroy_func = [](void* memory) {
            std::launder(reinterpret_cast<StoredFuncT*>(memory))->~StoredFuncT();
        };

        result.m_move_func = [](void* src, void* dst) {
            StoredFuncT* src_func = std::launder(reinterpret_cast<StoredFuncT*>(src));
            new (dst) StoredFuncT(std::move(*src_func));
            src_func->~StoredFuncT();
        };

        return result;
    }

    template <typename Func, typename... Args>
        requires(sizeof...(Args) > 0)
    static inplace_function create(Func&& func, Args&&... args)
    {
        return create([func = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
            std::invoke(func, args...);
        });
    }

    void operator()()
    {
        MIZU_ASSERT(is_valid(), "Trying to execute invalid inplace_function");
        m_invoke_func(&m_memory);
    }

    bool is_valid() const { return m_invoke_func != nullptr; }

  private:
    alignas(std::max_align_t) std::byte m_memory[CapacityBytes];

    void (*m_invoke_func)(void*) = nullptr;
    void (*m_destroy_func)(void*) = nullptr;
    void (*m_move_func)(void*, void*) = nullptr;

    void reset()
    {
        if (m_destroy_func != nullptr)
        {
            m_destroy_func(&m_memory);
            m_invoke_func = nullptr;
            m_destroy_func = nullptr;
            m_move_func = nullptr;
        }
    }

    void move_from(inplace_function&& other)
    {
        if (other.m_move_func != nullptr)
        {
            other.m_move_func(&other.m_memory, &m_memory);
            m_invoke_func = other.m_invoke_func;
            m_destroy_func = other.m_destroy_func;
            m_move_func = other.m_move_func;

            other.m_invoke_func = nullptr;
            other.m_destroy_func = nullptr;
            other.m_move_func = nullptr;
        }
    }
};

} // namespace Mizu
