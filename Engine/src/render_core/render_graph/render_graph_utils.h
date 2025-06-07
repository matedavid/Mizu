#pragma once

#include <functional>

#include "render_core/render_graph/render_graph_builder.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render_core/shader/shader_declaration.h"
#include "render_core/shader/shader_group.h"
#include "render_core/shader/shader_parameters.h"

namespace Mizu
{

// Forward declarations
class RenderCommandBuffer;

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

void bind_resource_group(RenderCommandBuffer& command,
                         const RGPassResources& resources,
                         const RGResourceGroupRef& ref,
                         uint32_t set);

template <typename ParamsT>
    requires IsValidParametersType<ParamsT>
void add_graphics_pass(RenderGraphBuilder& builder,
                       const std::string& name,
                       const GraphicsShaderDeclaration& shader,
                       const ParamsT& params,
                       const GraphicsPipeline::Description& pipeline_desc,
                       const RGFunction2& func)
{
    const GraphicsShaderDeclaration::ShaderDescription& shader_desc = shader.get_shader_description();

    GraphicsPipeline::Description local_pipeline_desc = pipeline_desc;
    local_pipeline_desc.vertex_shader = shader_desc.vertex;
    local_pipeline_desc.fragment_shader = shader_desc.fragment;

    ShaderGroup shader_group;
    shader_group.add_shader(*local_pipeline_desc.vertex_shader);
    shader_group.add_shader(*local_pipeline_desc.fragment_shader);

    std::vector<RGResourceGroupLayout> resource_group_layouts(shader_group.get_max_set());
    for (const ShaderParameterMemberInfo& info : ParamsT::get_members(params))
    {
        // TODO: This check is done multiple times, abstract this into a function like `is_framebuffer_parameter`
        if (info.mem_name == "framebuffer" && info.mem_type == ShaderParameterMemberType::RGFramebufferAttachments)
        {
            continue;
        }

        const ShaderPropertyBindingInfo binding_info = shader_group.get_property_binding_info(info.mem_name);
        const ShaderType stage = shader_group.get_resource_stage_bits(info.mem_name);

        std::visit(
            [&](auto&& value) {
                resource_group_layouts[binding_info.set].add_resource(binding_info.binding, value, stage);
            },
            info.value);
    }

    std::vector<RGResourceGroupRef> resource_group_refs(shader_group.get_max_set());
    for (uint32_t set = 0; set < resource_group_layouts.size(); ++set)
    {
        const RGResourceGroupLayout& layout = resource_group_layouts[set];

        resource_group_refs[set] = RGResourceGroupRef::invalid();

        if (layout.get_resources().empty())
            continue;

        resource_group_refs[set] = builder.create_resource_group(layout);
    }

    builder.add_pass(
        name, params, RGPassHint::Graphics, [=](RenderCommandBuffer& command, const RGPassResources& resources) {
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

} // namespace Mizu