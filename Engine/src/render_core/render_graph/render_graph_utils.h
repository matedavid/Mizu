#pragma once

#include <functional>

#include "render_core/render_graph/render_graph_builder.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/rtx/ray_tracing_pipeline.h"

#include "render_core/shader/shader_declaration.h"
#include "render_core/shader/shader_group.h"
#include "render_core/shader/shader_parameters.h"

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
void add_graphics_pass(
    RenderGraphBuilder& builder,
    const std::string& name,
    const GraphicsShaderDeclaration& shader,
    const ParamsT& params,
    const GraphicsPipeline::Description& pipeline_desc,
    const RGFunction& func)
{
    const GraphicsShaderDeclaration::ShaderDescription& shader_desc = shader.get_shader_description();

    GraphicsPipeline::Description local_pipeline_desc = pipeline_desc;
    local_pipeline_desc.vertex_shader = shader_desc.vertex;
    local_pipeline_desc.fragment_shader = shader_desc.fragment;

    ShaderGroup shader_group;
    shader_group.add_shader(*local_pipeline_desc.vertex_shader);
    shader_group.add_shader(*local_pipeline_desc.fragment_shader);

    std::vector<RGResourceGroupRef> resource_group_refs;
    create_resource_groups(builder, ParamsT::get_members(params), shader_group, resource_group_refs);

    builder.add_pass(name, params, RGPassHint::Graphics, [=](CommandBuffer& command, const RGPassResources& resources) {
        RenderPass::Description render_pass_desc{};
        render_pass_desc.target_framebuffer = resources.get_framebuffer();

        auto render_pass = Mizu::RenderPass::create(render_pass_desc);

        command.begin_render_pass(render_pass);
        {
            RHIHelpers::set_pipeline_state(command, local_pipeline_desc);

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
    const ComputeShaderDeclaration& shader,
    const ParamsT& params,
    const RGFunction& func)
{
    const ComputeShaderDeclaration::ShaderDescription& shader_desc = shader.get_shader_description();

    ComputePipeline::Description pipeline_desc{};
    pipeline_desc.shader = shader_desc.compute;

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
    const RayTracingShaderDeclaration& shader,
    const ParamsT& params,
    const RGFunction& func)
{
    const RayTracingShaderDeclaration::ShaderDescription& shader_desc = shader.get_shader_description();

    RayTracingPipeline::Description pipeline_desc = RayTracingShaderDeclaration::get_pipeline_template(shader_desc);
    // TODO: Should be able to configure somehow...
    // pipeline_desc.max_ray_recursion_depth

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

#define MIZU_RG_ADD_COMPUTE_PASS(_builder, _name, _shader, _params, _group_count)                    \
    Mizu::add_compute_pass(                                                                          \
        _builder,                                                                                    \
        _name,                                                                                       \
        _shader,                                                                                     \
        _params,                                                                                     \
        [=](Mizu::CommandBuffer& command, [[maybe_unused]] const Mizu::RGPassResources& resources) { \
            command.dispatch(_group_count);                                                          \
        })

} // namespace Mizu