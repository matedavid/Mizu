#pragma once

#include <functional>

namespace Mizu
{

class RGScopedGPUDebugLabel
{
  public:
    RGScopedGPUDebugLabel(std::function<void()> func) : m_func(std::move(func)) {}
    ~RGScopedGPUDebugLabel() { m_func(); }

  private:
    std::function<void()> m_func;
};

#define MIZU_RG_SCOPED_GPU_DEBUG_LABEL(builder, name) \
    builder.start_debug_label(name);                  \
    RGScopedGPUDebugLabel _scoped_gpu_debug_label([&builder]() { builder.end_debug_label(); })

} // namespace Mizu