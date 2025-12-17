#pragma once

#include <functional>

#include "mizu_render_core_module.h"
#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_shader_parameters.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/rhi_helpers.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;

class RGScopedGPUDebugLabel
{
  public:
    RGScopedGPUDebugLabel(std::function<void()> func) : m_func(std::move(func)) {}
    ~RGScopedGPUDebugLabel() { m_func(); }

  private:
    std::function<void()> m_func;
};

#define MIZU_RG_SCOPED_GPU_MARKER(builder, name) \
    builder.begin_gpu_marker(name);              \
    RGScopedGPUDebugLabel _scoped_gpu_debug_label([&builder]() { builder.end_gpu_marker(); })

void MIZU_RENDER_CORE_API bind_resource_group(
    CommandBuffer& command,
    const RGPassResources& resources,
    const RGResourceGroupRef& ref,
    uint32_t set);

#define MIZU_RG_ADD_COMPUTE_PASS(_builder, _name, _params, _pipeline, _group_count)                  \
    Mizu::add_compute_pass(                                                                          \
        _builder,                                                                                    \
        _name,                                                                                       \
        _params,                                                                                     \
        _pipeline,                                                                                   \
        [=](Mizu::CommandBuffer& command, [[maybe_unused]] const Mizu::RGPassResources& resources) { \
            command.dispatch(_group_count);                                                          \
        })

} // namespace Mizu