#include "fence.h"

#include "base/debug/logging.h"

namespace Mizu
{

ThreadFence::ThreadFence() : ThreadFence(false) {}

ThreadFence::ThreadFence(bool default_signaled) : m_is_signaled(default_signaled) {}

ThreadFence& ThreadFence::operator=(const ThreadFence& other)
{
    if (this != &other)
    {
        m_is_signaled.store(other.m_is_signaled.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    return *this;
}

bool ThreadFence::is_signaled() const
{
    return m_is_signaled.load(std::memory_order_relaxed);
}

void ThreadFence::reset()
{
#if MIZU_DEBUG
    if (!is_signaled())
    {
        MIZU_LOG_WARNING("ThreadFence is not signaled");
    }
#endif

    m_is_signaled.store(false, std::memory_order_relaxed);
}

void ThreadFence::signal()
{
#if MIZU_DEBUG
    if (is_signaled())
    {
        MIZU_LOG_WARNING("ThreadFence is already signaled");
        return;
    }
#endif

    m_is_signaled.store(true, std::memory_order_relaxed);
}

} // namespace Mizu