#pragma once

#include <memory>

namespace Mizu
{

class Fence
{
  public:
    virtual ~Fence() = default;

    static std::shared_ptr<Fence> create();
    static std::shared_ptr<Fence> create(bool signaled);

    virtual void wait_for() = 0;
};

class Semaphore
{
  public:
    virtual ~Semaphore() = default;

    static std::shared_ptr<Semaphore> create();
};

} // namespace Mizu
