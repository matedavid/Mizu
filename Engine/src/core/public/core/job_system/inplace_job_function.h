#pragma once

#include <type_traits>

namespace Mizu
{

constexpr size_t InplaceJobMemoryBytes = 48;

class InplaceJobFunction
{
  public:
    InplaceJobFunction() = default;
    InplaceJobFunction(InplaceJobFunction&& other) { move_from(std::move(other)); }

    InplaceJobFunction& operator=(InplaceJobFunction&& other)
    {
        if (this != &other)
        {
            reset();
            move_from(std::move(other));
        }

        return *this;
    }

    InplaceJobFunction(const InplaceJobFunction&) = delete;
    InplaceJobFunction& operator=(const InplaceJobFunction&) = delete;
    ~InplaceJobFunction() { reset(); }

    template <typename Func>
    static InplaceJobFunction create(Func&& func)
    {
        using StoredFuncT = std::decay_t<Func>;

        static_assert(sizeof(StoredFuncT) <= InplaceJobMemoryBytes, "Template function too large for inplace job");
        static_assert(
            alignof(StoredFuncT) <= alignof(MemoryT), "Template function alignment too large for inplace job");

        InplaceJobFunction result{};
        new (&result.m_memory) StoredFuncT(std::forward<Func>(func));

        result.m_invoke_func = [](void* memory) {
            StoredFuncT& func = *reinterpret_cast<StoredFuncT*>(memory);
            std::invoke(func);
        };

        result.m_destroy_func = [](void* memory) {
            reinterpret_cast<StoredFuncT*>(memory)->~StoredFuncT();
        };

        result.m_move_func = [](void* src, void* dst) {
            new (dst) StoredFuncT(std::move(*reinterpret_cast<StoredFuncT*>(src)));
            reinterpret_cast<StoredFuncT*>(src)->~StoredFuncT();
        };

        return result;
    }

    template <typename Func, typename... Args>
    static InplaceJobFunction create(Func&& func, Args&&... args)
    {
        return create([func = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
            std::invoke(func, args...);
        });
    }

    void operator()()
    {
        MIZU_ASSERT(is_valid(), "Trying to execute invalid InplaceJobFunction");
        m_invoke_func(&m_memory);
    }

    bool is_valid() const { return m_invoke_func != nullptr; }

  private:
    using MemoryT = std::aligned_storage_t<InplaceJobMemoryBytes, alignof(std::max_align_t)>;
    MemoryT m_memory;

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

    void move_from(InplaceJobFunction&& other)
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
