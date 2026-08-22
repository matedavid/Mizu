#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "base/debug/assert.h"

namespace Mizu
{

// clang-format off
template <typename T, size_t CapacityBytes>
concept InplaceAnyStorable = std::is_same_v<T, std::decay_t<T>>
                          && std::is_copy_constructible_v<T>
                          && sizeof(T) <= CapacityBytes
                          && alignof(T) <= alignof(std::max_align_t);
// clang-format on

template <size_t CapacityBytes>
class inplace_any
{
  public:
    inplace_any() = default;
    ~inplace_any() { reset(); }

    inplace_any(const inplace_any& other) { copy_from(other); }

    inplace_any(inplace_any&& other) { move_from(other); }

    template <typename T>
        requires(!std::is_same_v<std::decay_t<T>, inplace_any>) && InplaceAnyStorable<std::decay_t<T>, CapacityBytes>
    inplace_any(T&& value)
    {
        emplace<std::decay_t<T>>(std::forward<T>(value));
    }

    inplace_any& operator=(const inplace_any& other)
    {
        if (this != &other)
        {
            reset();
            copy_from(other);
        }

        return *this;
    }

    inplace_any& operator=(inplace_any&& other)
    {
        if (this != &other)
        {
            reset();
            move_from(other);
        }

        return *this;
    }

    template <typename T>
        requires(!std::is_same_v<std::decay_t<T>, inplace_any>) && InplaceAnyStorable<std::decay_t<T>, CapacityBytes>
    inplace_any& operator=(T&& value)
    {
        reset();
        emplace<std::decay_t<T>>(std::forward<T>(value));

        return *this;
    }

    template <InplaceAnyStorable<CapacityBytes> T, typename... Args>
    T& emplace(Args&&... args)
    {
        reset();

        T* value = new (static_cast<void*>(m_memory)) T(std::forward<Args>(args)...);
        m_ops = ops_for<T>();

        return *value;
    }

    void reset()
    {
        if (m_ops != nullptr)
        {
            m_ops->destroy(m_memory);
            m_ops = nullptr;
        }
    }

    bool has_value() const { return m_ops != nullptr; }

    constexpr size_t capacity() const { return CapacityBytes; }

    template <InplaceAnyStorable<CapacityBytes> T>
    bool is_type() const
    {
        return m_ops != nullptr && *m_ops->type == typeid(T);
    }

    template <InplaceAnyStorable<CapacityBytes> T>
    T* get_if()
    {
        return is_type<T>() ? std::launder(reinterpret_cast<T*>(m_memory)) : nullptr;
    }

    template <InplaceAnyStorable<CapacityBytes> T>
    const T* get_if() const
    {
        return is_type<T>() ? std::launder(reinterpret_cast<const T*>(m_memory)) : nullptr;
    }

    template <InplaceAnyStorable<CapacityBytes> T>
    T& get()
    {
        T* value = get_if<T>();
        MIZU_ASSERT(value != nullptr, "inplace_any does not hold a {}", typeid(T).name());

        return *value;
    }

    template <InplaceAnyStorable<CapacityBytes> T>
    const T& get() const
    {
        const T* value = get_if<T>();
        MIZU_ASSERT(value != nullptr, "inplace_any does not hold a {}", typeid(T).name());

        return *value;
    }

  private:
    struct OpsT
    {
        const std::type_info* type;
        void (*destroy)(std::byte*);
        void (*copy)(const std::byte*, std::byte*);
        void (*move)(std::byte*, std::byte*);
    };

    alignas(std::max_align_t) std::byte m_memory[CapacityBytes];
    const OpsT* m_ops = nullptr;

    template <typename T>
    static const OpsT* ops_for()
    {
        static constexpr OpsT ops{
            .type = &typeid(T),
            .destroy = [](std::byte* p) { std::launder(reinterpret_cast<T*>(p))->~T(); },
            .copy = [](const std::byte* src,
                       std::byte* dst) { new (dst) T(*std::launder(reinterpret_cast<const T*>(src))); },
            .move = [](std::byte* src,
                       std::byte* dst) { new (dst) T(std::move(*std::launder(reinterpret_cast<T*>(src)))); },
        };

        return &ops;
    }

    void copy_from(const inplace_any& other)
    {
        if (other.m_ops != nullptr)
        {
            other.m_ops->copy(other.m_memory, m_memory);
            m_ops = other.m_ops;
        }
    }

    void move_from(inplace_any& other)
    {
        if (other.m_ops != nullptr)
        {
            other.m_ops->move(other.m_memory, m_memory);
            m_ops = other.m_ops;
            other.reset();
        }
    }
};

} // namespace Mizu