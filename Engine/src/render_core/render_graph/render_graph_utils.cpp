#include "render_graph_utils.h"

#include "render_core/render_graph/render_graph_resources.h"

#include "render_core/rhi/resource_group.h"

namespace Mizu
{

void bind_resource_group(
    CommandBuffer& command,
    const RGPassResources& resources,
    const RGResourceGroupRef& ref,
    uint32_t set)
{
    const std::shared_ptr<ResourceGroup>& resource_group = resources.get_resource_group(ref);
    MIZU_ASSERT(resource_group != nullptr, "Invalid ResourceGroup");

    command.bind_resource_group(resource_group, set);
}

void create_resource_groups(
    RenderGraphBuilder& builder,
    const std::vector<ShaderParameterMemberInfo>& members,
    const ShaderGroup& shader_group,
    std::vector<RGResourceGroupRef>& resource_group_refs)
{
    std::vector<RGResourceGroupLayout> resource_group_layouts(shader_group.get_max_set());
    for (const ShaderParameterMemberInfo& info : members)
    {
        // TODO: This check is done multiple times, abstract this into a function like `is_framebuffer_parameter`
        if (info.mem_name == "framebuffer" && info.mem_type == ShaderParameterMemberType::RGFramebufferAttachments)
        {
            continue;
        }

        const ShaderResource& parameter = shader_group.get_parameter_info(info.mem_name);
        const ShaderType stage = shader_group.get_resource_stage_bits(info.mem_name);

        if (std::holds_alternative<ShaderResourceTexture>(parameter.value))
        {
            const ShaderResourceTexture& texture = std::get<ShaderResourceTexture>(parameter.value);

            const RGImageViewRef value = std::get<RGImageViewRef>(info.value);

            // TODO: Refactor this once resource group layouts take into account d3d12 implementation
            switch (texture.access)
            {
            case ShaderResourceAccessType::ReadOnly:
                resource_group_layouts[parameter.binding_info.set].add_resource(
                    parameter.binding_info.binding, value, stage, ShaderImageProperty::Type::Sampled);
                break;
            case ShaderResourceAccessType::ReadWrite:
                resource_group_layouts[parameter.binding_info.set].add_resource(
                    parameter.binding_info.binding, value, stage, ShaderImageProperty::Type::Storage);
                break;
            }
        }
        else if (std::holds_alternative<ShaderResourceStructuredBuffer>(parameter.value))
        {
            // TODO: Refactor this once resource group layouts take into account d3d12 implementation

            // const ShaderResourceStructuredBuffer& structured_buffer =
            //     std::get<ShaderResourceStructuredBuffer>(parameter.value);

            resource_group_layouts[parameter.binding_info.set].add_resource(
                parameter.binding_info.binding,
                std::get<RGStorageBufferRef>(info.value),
                stage,
                ShaderBufferProperty::Type::Storage);
        }
        else if (std::holds_alternative<ShaderResourceConstantBuffer>(parameter.value))
        {
            resource_group_layouts[parameter.binding_info.set].add_resource(
                parameter.binding_info.binding,
                std::get<RGUniformBufferRef>(info.value),
                stage,
                ShaderBufferProperty::Type::Uniform);
        }
        else if (std::holds_alternative<ShaderResourceSamplerState>(parameter.value))
        {
            const auto& value = std::get<std::shared_ptr<SamplerState>>(info.value);
            resource_group_layouts[parameter.binding_info.set].add_resource(
                parameter.binding_info.binding, value, stage);
        }
        else if (std::holds_alternative<ShaderResourceAccelerationStructure>(parameter.value))
        {
            const auto& value = std::get<RGAccelerationStructureRef>(info.value);
            resource_group_layouts[parameter.binding_info.set].add_resource(
                parameter.binding_info.binding, value, stage);
        }
        else
        {
            MIZU_UNREACHABLE("Invalid ShaderProperty or not implemented")
        }
    }

    resource_group_refs = std::vector<RGResourceGroupRef>(shader_group.get_max_set());
    for (uint32_t set = 0; set < resource_group_layouts.size(); ++set)
    {
        const RGResourceGroupLayout& layout = resource_group_layouts[set];

        resource_group_refs[set] = RGResourceGroupRef::invalid();

        if (layout.get_resources().empty())
            continue;

        resource_group_refs[set] = builder.create_resource_group(layout);
    }
}

} // namespace Mizu