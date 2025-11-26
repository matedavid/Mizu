#pragma once

#include <functional>

#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_shader_parameters.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/rtx/ray_tracing_pipeline.h"
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

template <typename ParamsT>
    requires IsValidParametersType<ParamsT>
void add_raster_pass(
    RenderGraphBuilder& builder,
    const std::string& name,
    const ParamsT& params,
    const GraphicsPipeline::Description& pipeline_desc,
    const RGFunction& func)
{
    ShaderGroup shader_group;
    shader_group.add_shader(*pipeline_desc.vertex_shader);
    shader_group.add_shader(*pipeline_desc.fragment_shader);

    std::vector<RGResourceGroupRef> resource_group_refs;
    create_resource_groups(builder, ParamsT::get_members(params), shader_group, resource_group_refs);

    builder.add_pass(name, params, RGPassHint::Raster, [=](CommandBuffer& command, const RGPassResources& resources) {
        const auto framebuffer = resources.get_framebuffer();
        command.begin_render_pass(framebuffer);
        {
            RHIHelpers::set_pipeline_state(command, pipeline_desc);

            for (uint32_t set = 0; set < resource_group_refs.size(); ++set)
            {
                const RGResourceGroupRef& ref = resource_group_refs[set];

                if (ref != RGResourceGroupRef::invalid())
                {
                    bind_resource_group(command, resources, ref, set);
                }
            }

            func(command, resources);
        }
        command.end_render_pass();
    });
}

template <typename ParamsT>
    requires IsValidParametersType<ParamsT>
void add_compute_pass(
    RenderGraphBuilder& builder,
    const std::string& name,
    const ParamsT& params,
    const ComputePipeline::Description& pipeline_desc,
    const RGFunction& func)
{
    ShaderGroup shader_group;
    shader_group.add_shader(*pipeline_desc.shader);

    std::vector<RGResourceGroupRef> resource_group_refs;
    create_resource_groups(builder, ParamsT::get_members(params), shader_group, resource_group_refs);

    builder.add_pass(name, params, RGPassHint::Compute, [=](CommandBuffer& command, const RGPassResources& resources) {
        RHIHelpers::set_pipeline_state(command, pipeline_desc);

        for (uint32_t set = 0; set < resource_group_refs.size(); ++set)
        {
            const RGResourceGroupRef& ref = resource_group_refs[set];

            if (ref != RGResourceGroupRef::invalid())
            {
                bind_resource_group(command, resources, ref, set);
            }
        }

        func(command, resources);
    });
}

template <typename ParamsT>
    requires IsValidParametersType<ParamsT>
void add_rtx_pass(
    RenderGraphBuilder& builder,
    const std::string& name,
    const ParamsT& params,
    const RayTracingPipeline::Description& pipeline_desc,
    const RGFunction& func)
{
    ShaderGroup shader_group;
    shader_group.add_shader(*pipeline_desc.raygen_shader);
    for (const auto& miss_shader : pipeline_desc.miss_shaders)
        shader_group.add_shader(*miss_shader);
    for (const auto& closest_hit_shader : pipeline_desc.closest_hit_shaders)
        shader_group.add_shader(*closest_hit_shader);

    std::vector<RGResourceGroupRef> resource_group_refs;
    create_resource_groups(builder, ParamsT::get_members(params), shader_group, resource_group_refs);

    builder.add_pass(
        name, params, RGPassHint::RayTracing, [=](CommandBuffer& command, const RGPassResources& resources) {
            RHIHelpers::set_pipeline_state(command, pipeline_desc);

            for (uint32_t set = 0; set < resource_group_refs.size(); ++set)
            {
                const RGResourceGroupRef& ref = resource_group_refs[set];

                if (ref != RGResourceGroupRef::invalid())
                {
                    bind_resource_group(command, resources, ref, set);
                }
            }

            func(command, resources);
        });
}

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