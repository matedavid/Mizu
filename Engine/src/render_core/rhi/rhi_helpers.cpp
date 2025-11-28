#include "rhi_helpers.h"

#include "renderer/material/material.h"
#include "renderer/model/mesh.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

std::shared_ptr<SamplerState> RHIHelpers::get_sampler_state(const SamplerStateDescription& options)
{
    return Renderer::get_sampler_state_cache()->get_sampler_state(options);
}

void RHIHelpers::draw_mesh(CommandBuffer& command, const Mesh& mesh)
{
    draw_mesh_instanced(command, mesh, 1);
}

void RHIHelpers::draw_mesh_instanced(CommandBuffer& command, const Mesh& mesh, uint32_t instance_count)
{
    const std::string& mesh_name = mesh.get_name();

    if (!mesh_name.empty())
        command.begin_gpu_marker(mesh_name);

    command.draw_indexed_instanced(*mesh.vertex_buffer(), *mesh.index_buffer(), instance_count);

    if (!mesh_name.empty())
        command.end_gpu_marker();
}

#if MIZU_DEBUG

static void validate_graphics_pipeline_compatible_with_framebuffer(const Shader& shader, const Framebuffer& framebuffer)
{
    inplace_vector<ImageFormat, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> framebuffer_formats;

    for (const Framebuffer::Attachment& attachment : framebuffer.get_color_attachments())
    {
        const ImageFormat format = attachment.rtv->get_format();
        framebuffer_formats.push_back(format);
    }

    const std::span<const ShaderInputOutput> outputs = shader.get_reflection().get_outputs();
    MIZU_ASSERT(
        outputs.size() == framebuffer_formats.size(),
        "Number of shader outputs ({}) and framebuffer color attachments ({}) does not match",
        outputs.size(),
        framebuffer_formats.size());

    const auto is_image_format_compatible_with_shader_value_type = [](ImageFormat format,
                                                                      ShaderPrimitiveType type) -> bool {
        // What makes ImageFormat <-> ShaderValueType compatible?
        // - Has the same number of "components"

        return ImageUtils::get_num_components(format) == ShaderPrimitiveType::num_components(type);
    };

    for (size_t i = 0; i < outputs.size(); ++i)
    {
        const ImageFormat& format = framebuffer_formats[i];
        const ShaderInputOutput& output = outputs[i];

        MIZU_ASSERT(
            is_image_format_compatible_with_shader_value_type(format, output.primitive.type),
            "Shader output and framebuffer attachment at idx {} are not compatible",
            i);
    }
}

#endif

void RHIHelpers::set_pipeline_state(CommandBuffer& command, const GraphicsPipelineDescription& pipeline_desc)
{
    MIZU_ASSERT(command.get_active_framebuffer() != nullptr, "CommandBuffer has no bound RenderPass");

    GraphicsPipelineDescription local_desc = pipeline_desc;
    local_desc.target_framebuffer = command.get_active_framebuffer();

    const auto& pipeline = Renderer::get_pipeline_cache()->get_pipeline(local_desc);
    MIZU_ASSERT(pipeline != nullptr, "GraphicsPipeline is nullptr");

#if MIZU_DEBUG
    validate_graphics_pipeline_compatible_with_framebuffer(
        *pipeline_desc.fragment_shader, *local_desc.target_framebuffer);
#endif

    command.bind_pipeline(pipeline);
}

void RHIHelpers::set_pipeline_state(CommandBuffer& command, const ComputePipelineDescription& pipeline_desc)
{
    MIZU_ASSERT(
        command.get_active_framebuffer() == nullptr,
        "Can't set ComputePipeline state if CommandBuffer has an active RenderPass");

    const auto& pipeline = Renderer::get_pipeline_cache()->get_pipeline(pipeline_desc);
    MIZU_ASSERT(pipeline != nullptr, "ComputePipeline is nullptr");

    command.bind_pipeline(pipeline);
}

void RHIHelpers::set_pipeline_state(CommandBuffer& command, const RayTracingPipelineDescription& pipeline_desc)
{
    MIZU_ASSERT(
        command.get_active_framebuffer() == nullptr,
        "Can't set RayTracingPipeline state if CommandBuffer has an active RenderPass");

    const auto& pipeline = Renderer::get_pipeline_cache()->get_pipeline(pipeline_desc);
    MIZU_ASSERT(pipeline != nullptr, "ComputePipeline is nullptr");

    command.bind_pipeline(pipeline);
}

void RHIHelpers::set_material(CommandBuffer& command, const Material& material)
{
    MIZU_ASSERT(command.get_active_framebuffer() != nullptr, "CommandBuffer has no bound RenderPass");
    MIZU_ASSERT(material.is_baked(), "Material has not been baked");

    for (const MaterialResourceGroup& material_rg : material.get_resource_groups())
    {
        command.bind_resource_group(material_rg.resource_group, material_rg.set);
    }
}

glm::uvec3 RHIHelpers::compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size)
{
    return {
        (thread_count.x + group_size.x - 1) / group_size.x,
        (thread_count.y + group_size.y - 1) / group_size.y,
        (thread_count.z + group_size.z - 1) / group_size.z};
}

} // namespace Mizu
