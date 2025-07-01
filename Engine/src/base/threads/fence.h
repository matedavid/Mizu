#pragma once

#include <thread>

namespace Mizu
{

class ThreadFence
{
  public:
    ThreadFence();
    ThreadFence(bool default_open);

    ThreadFence& operator=(const ThreadFence& other);

    bool is_signaled() const;

    void reset();
    void signal();

  private:
    std::atomic<bool> m_is_signaled;
};

} // namespace Mizu