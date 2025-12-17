#pragma once

#include <memory>

#include "mizu_render_core_module.h"

namespace Mizu
{

class MIZU_RENDER_CORE_API Fence
{
  public:
    virtual ~Fence() = default;

    static std::shared_ptr<Fence> create();
    static std::shared_ptr<Fence> create(bool signaled);

    virtual void wait_for() = 0;
};

class MIZU_RENDER_CORE_API Semaphore
{
  public:
    virtual ~Semaphore() = default;

    static std::shared_ptr<Semaphore> create();
};

} // namespace Mizu
