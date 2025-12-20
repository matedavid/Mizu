#pragma once

#include <memory>

#include "mizu_render_core_module.h"

namespace Mizu
{

class MIZU_RENDER_CORE_API Fence
{
  public:
    virtual ~Fence() = default;

    virtual void wait_for() = 0;
};

class MIZU_RENDER_CORE_API Semaphore
{
  public:
    virtual ~Semaphore() = default;
};

} // namespace Mizu
