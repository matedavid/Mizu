#pragma once

#include <memory>

#include "mizu_render_core_module.h"

namespace Mizu
{

class Fence
{
  public:
    virtual ~Fence() = default;

    virtual void wait_for() = 0;
};

class Semaphore
{
  public:
    virtual ~Semaphore() = default;
};

} // namespace Mizu
