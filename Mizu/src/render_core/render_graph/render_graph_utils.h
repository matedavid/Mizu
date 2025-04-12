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

#define MIZU_RG_ADD_COMPUTE_PASS(_builder, _name, _shader, _params, _group_count) \
    _builder.add_pass(_name, _shader, _params, [=](RenderCommandBuffer& command) { command.dispatch(_group_count); })

} // namespace Mizu