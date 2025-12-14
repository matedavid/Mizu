#pragma once

#include <functional>

#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_shader_parameters.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/shader/shader_group.h"

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

void bind_resource_group(
    CommandBuffer& command,
    const RGPassResources& resources,
    const RGResourceGroupRef& ref,
    uint32_t set);

void create_resource_groups(
    RenderGraphBuilder& builder,
    const std::vector<ShaderParameterMemberInfo>& members,
    const ShaderGroup& shader_group,
    std::vector<RGResourceGroupRef>& resource_group_refs);

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