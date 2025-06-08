#include "rhi_helpers.h"

#include "render_core/resources/material.h"
#include "render_core/resources/mesh.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

std::shared_ptr<SamplerState> RHIHelpers::get_sampler_state(const SamplingOptions& options)
{
    return Renderer::get_sampler_state_cache()->get_sampler_state(options);
}

void RHIHelpers::draw_mesh(RenderCommandBuffer& command, const Mesh& mesh)
{
    command.draw_indexed(*mesh.vertex_buffer(), *mesh.index_buffer());
}

static void validate_graphics_pipeline_compatible_with_framebuffer(const GraphicsShader& shader,
                                                                   const Framebuffer& framebuffer)
{
    std::vector<ImageFormat> framebuffer_formats;

    for (const Framebuffer::Attachment& attachment : framebuffer.get_attachments())
    {
        const ImageFormat format = attachment.image_view->get_format();
        if (!ImageUtils::is_depth_format(format))
        {
            framebuffer_formats.push_back(format);
        }
    }

    const std::vector<ShaderOutput>& outputs = shader.get_outputs();

    MIZU_ASSERT(outputs.size() == framebuffer_formats.size(),
                "Number fo shader outputs ({}) and framebuffer color attachments ({}) does not match",
                outputs.size(),
                framebuffer_formats.size());

    const auto is_image_format_compatible_with_shader_value_type = [](ImageFormat format,
                                                                      ShaderValueType type) -> bool {
        // What makes ImageFormat <-> ShaderValueType compatible?
        // - Has the same number of "components"

        return ImageUtils::get_num_components(format) == ShaderValueType::num_components(type);
    };

    for (size_t i = 0; i < outputs.size(); ++i)
    {
        const ImageFormat& format = framebuffer_formats[i];
        const ShaderOutput& output = outputs[i];

        MIZU_ASSERT(is_image_format_compatible_with_shader_value_type(format, output.type),
                    "Shader output and framebuffer attachment at idx {} are not compatible",
                    i);
    }
}

void RHIHelpers::set_pipeline_state(RenderCommandBuffer& command, const GraphicsPipeline::Description& pipeline_desc)
{
    MIZU_ASSERT(command.get_current_render_pass() != nullptr, "RenderCommandBuffer has no bound RenderPass");

    GraphicsPipeline::Description local_desc = pipeline_desc;
    local_desc.target_framebuffer = command.get_current_render_pass()->get_framebuffer();

    const auto& pipeline = Renderer::get_pipeline_cache()->get_pipeline(local_desc);
    MIZU_ASSERT(pipeline != nullptr, "Pipeline is nullptr");

#if MIZU_DEBUG
    validate_graphics_pipeline_compatible_with_framebuffer(*pipeline_desc.shader, *local_desc.target_framebuffer);
#endif

    command.bind_pipeline(pipeline);
}

void RHIHelpers::set_pipeline_state(RenderCommandBuffer& command, const ComputePipeline::Description& pipeline_desc)
{
    MIZU_ASSERT(command.get_current_render_pass() == nullptr,
                "Can't set ComputePipeline state if RenderCommandBuffer has an active RenderPass");

    const auto& pipeline = Renderer::get_pipeline_cache()->get_pipeline(pipeline_desc);
    MIZU_ASSERT(pipeline != nullptr, "ComputePipeline is nullptr");

    command.bind_pipeline(pipeline);
}

void RHIHelpers::set_material(RenderCommandBuffer& command,
                              const Material& material,
                              const GraphicsPipeline::Description& pipeline_desc)
{
    MIZU_ASSERT(command.get_current_render_pass() != nullptr, "RenderCommandBuffer has no bound RenderPass");
    MIZU_ASSERT(material.is_baked(), "Material has not been baked");

    GraphicsPipeline::Description local_desc = pipeline_desc;
    local_desc.vertex_shader = material.get_vertex_shader();
    local_desc.fragment_shader = material.get_fragment_shader();

    set_pipeline_state(command, local_desc);

    for (const MaterialResourceGroup& material_rg : material.get_resource_groups())
    {
        command.bind_resource_group(material_rg.resource_group, material_rg.set);
    }
}

glm::uvec3 RHIHelpers::compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size)
{
    return {(thread_count.x + group_size.x - 1) / group_size.x,
            (thread_count.y + group_size.y - 1) / group_size.y,
            (thread_count.z + group_size.z - 1) / group_size.z};
}

} // namespace Mizu
