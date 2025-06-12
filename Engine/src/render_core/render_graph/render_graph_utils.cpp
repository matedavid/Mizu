#include "render_graph_utils.h"

#include "render_core/render_graph/render_graph_resources.h"

#include "render_core/rhi/resource_group.h"

namespace Mizu
{

void bind_resource_group(CommandBuffer& command,
                         const RGPassResources& resources,
                         const RGResourceGroupRef& ref,
                         uint32_t set)
{
    const std::shared_ptr<ResourceGroup>& resource_group = resources.get_resource_group(ref);
    MIZU_ASSERT(resource_group != nullptr, "Invalid ResourceGroup");

    command.bind_resource_group(resource_group, set);
}

void create_resource_groups(RenderGraphBuilder& builder,
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

        const ShaderProperty& property = shader_group.get_property_info(info.mem_name);
        const ShaderType stage = shader_group.get_resource_stage_bits(info.mem_name);

        if (std::holds_alternative<ShaderImageProperty>(property.value))
        {
            const ShaderImageProperty& image_property = std::get<ShaderImageProperty>(property.value);

            const RGImageViewRef value = std::get<RGImageViewRef>(info.value);
            resource_group_layouts[property.binding_info.set].add_resource(
                property.binding_info.binding, value, stage, image_property.type);
        }
        else if (std::holds_alternative<ShaderBufferProperty>(property.value))
        {
            const ShaderBufferProperty& buffer_property = std::get<ShaderBufferProperty>(property.value);

            RGBufferRef value;
            if (std::holds_alternative<RGUniformBufferRef>(info.value))
                value = std::get<RGUniformBufferRef>(info.value);
            else if (std::holds_alternative<RGStorageBufferRef>(info.value))
                value = std::get<RGStorageBufferRef>(info.value);

            resource_group_layouts[property.binding_info.set].add_resource(
                property.binding_info.binding, value, stage, buffer_property.type);
        }
        else if (std::holds_alternative<ShaderSamplerProperty>(property.value))
        {
            const auto& value = std::get<std::shared_ptr<SamplerState>>(info.value);
            resource_group_layouts[property.binding_info.set].add_resource(property.binding_info.binding, value, stage);
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