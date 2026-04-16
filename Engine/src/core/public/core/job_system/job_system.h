#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"

#include "mizu_core_module.h"

namespace Mizu
{

constexpr size_t InplaceJobMemoryBytes = 48;
constexpr size_t MaxJobDependencies = 8;

enum class JobAffinity
{
    None,
    Main,
    Simulation,
    Render,
};

struct JobHandle
{
    static constexpr size_t InvalidIndex = std::numeric_limits<size_t>::max();

    size_t index = InvalidIndex;
    size_t generation = 0;

    bool is_valid() const { return index != InvalidIndex; }
};

class InplaceJobFunction
{
  public:
    InplaceJobFunction() = default;
    InplaceJobFunction(InplaceJobFunction&& other) noexcept { move_from(std::move(other)); }

    InplaceJobFunction& operator=(InplaceJobFunction&& other) noexcept
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

class Job2
{
  public:
    template <typename Func, typename... Args>
    static Job2 create(std::string_view name, Func&& func, Args&&... args)
    {
        Job2 job{};
        job.m_name = name;
        job.m_func =
            InplaceJobFunction::create([func = std::forward<Func>(func),
                                        ... args = std::forward<Args>(args)]() mutable { std::invoke(func, args...); });

        return job;
    }

    Job2& depends_on(JobHandle handle)
    {
        MIZU_ASSERT(m_dependencies.size() < MaxJobDependencies, "Too many dependencies");
        m_dependencies.push_back(handle);

        return *this;
    }

    Job2& set_affinity(JobAffinity affinity)
    {
        m_affinity = affinity;
        return *this;
    }

    void execute() { m_func(); }

    bool has_dependencies() const { return !m_dependencies.empty(); }

  private:
    std::string_view m_name{};

    InplaceJobFunction m_func{};
    inplace_vector<JobHandle, MaxJobDependencies> m_dependencies{};
    JobAffinity m_affinity = JobAffinity::None;

    friend class JobSystem2;
};

class MIZU_CORE_API JobSystem2
{
  public:
    JobSystem2();

    bool init(uint32_t num_workers);

    void schedule();

  private:
    void worker_job();
};

} // namespace Mizu
