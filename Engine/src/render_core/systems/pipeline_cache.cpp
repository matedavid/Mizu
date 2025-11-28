#include "pipeline_cache.h"

#include "base/debug/assert.h"

#include "render_core/rhi/framebuffer.h"

namespace Mizu
{

std::shared_ptr<Pipeline> PipelineCache::get_pipeline(const GraphicsPipelineDescription& desc)
{
    const size_t h = hash(desc);

    const auto it = m_pipeline_cache.find(h);
    if (it != m_pipeline_cache.end())
    {
        return it->second;
    }

    const auto pipeline = Pipeline::create(desc);
    MIZU_ASSERT(pipeline != nullptr, "Failed to create GraphicsPipeline");

    return m_pipeline_cache.insert({h, pipeline}).first->second;
}

std::shared_ptr<Pipeline> PipelineCache::get_pipeline(const ComputePipelineDescription& desc)
{
    const size_t h = hash(desc);

    const auto it = m_pipeline_cache.find(h);
    if (it != m_pipeline_cache.end())
    {
        return it->second;
    }

    const auto pipeline = Pipeline::create(desc);
    MIZU_ASSERT(pipeline != nullptr, "Failed to create ComputePipeline");

    return m_pipeline_cache.insert({h, pipeline}).first->second;
}

std::shared_ptr<Pipeline> PipelineCache::get_pipeline(const RayTracingPipelineDescription& desc)
{
    const size_t h = hash(desc);

    const auto it = m_pipeline_cache.find(h);
    if (it != m_pipeline_cache.end())
    {
        return it->second;
    }

    const auto pipeline = Pipeline::create(desc);
    MIZU_ASSERT(pipeline != nullptr, "Failed to create RayTracingPipeline");

    return m_pipeline_cache.insert({h, pipeline}).first->second;
}

size_t PipelineCache::hash(const GraphicsPipelineDescription& desc) const
{
    std::hash<bool> bool_hasher;
    std::hash<int32_t> int_hasher;
    std::hash<float> float_hasher;

    size_t h = 0;

    // Shader
    h ^= std::hash<Shader*>()(desc.vertex_shader.get());
    h ^= std::hash<Shader*>()(desc.fragment_shader.get());

    // Target framebuffer
    {
        for (const Framebuffer::Attachment& attachment : desc.target_framebuffer->get_color_attachments())
        {
            h ^= static_cast<size_t>(attachment.initial_state);
            h ^= static_cast<size_t>(attachment.final_state);
            h ^= static_cast<size_t>(attachment.load_operation);
            h ^= static_cast<size_t>(attachment.store_operation);
        }

        const std::optional<const Framebuffer::Attachment>& depth_stencil_attachment_opt =
            desc.target_framebuffer->get_depth_stencil_attachment();
        if (depth_stencil_attachment_opt.has_value())
        {
            const Framebuffer::Attachment& attachment = *depth_stencil_attachment_opt;

            h ^= static_cast<size_t>(attachment.initial_state);
            h ^= static_cast<size_t>(attachment.final_state);
            h ^= static_cast<size_t>(attachment.load_operation);
            h ^= static_cast<size_t>(attachment.store_operation);
        }
    }

    // Rasterization state
    {
        h ^= bool_hasher(desc.rasterization.rasterizer_discard);
        h ^= int_hasher(static_cast<int32_t>(desc.rasterization.polygon_mode));
        h ^= int_hasher(static_cast<int32_t>(desc.rasterization.cull_mode));
        h ^= int_hasher(static_cast<int32_t>(desc.rasterization.front_face));

        h ^= bool_hasher(desc.rasterization.depth_bias.enabled);
        h ^= float_hasher(desc.rasterization.depth_bias.constant_factor);
        h ^= float_hasher(desc.rasterization.depth_bias.clamp);
        h ^= float_hasher(desc.rasterization.depth_bias.slope_factor);
    }

    // Depth stencil state
    {
        h ^= bool_hasher(desc.depth_stencil.depth_test);
        h ^= bool_hasher(desc.depth_stencil.depth_write);
        h ^= int_hasher(static_cast<int32_t>(desc.depth_stencil.depth_compare_op));

        h ^= bool_hasher(desc.depth_stencil.depth_bounds_test);
        h ^= float_hasher(desc.depth_stencil.min_depth_bounds);
        h ^= float_hasher(desc.depth_stencil.max_depth_bounds);

        h ^= bool_hasher(desc.depth_stencil.stencil_test);
    }

    // Color blend state
    {
        h ^= int_hasher(static_cast<int32_t>(desc.color_blend.method));
        h ^= int_hasher(static_cast<int32_t>(desc.color_blend.logic_op));

        for (const ColorBlendState::AttachmentState& attachment : desc.color_blend.attachments)
        {
            h ^= bool_hasher(attachment.blend_enabled);
            h ^= int_hasher(static_cast<int32_t>(attachment.src_color_blend_factor));
            h ^= int_hasher(static_cast<int32_t>(attachment.dst_color_blend_factor));
            h ^= int_hasher(static_cast<int32_t>(attachment.color_blend_op));
            h ^= int_hasher(static_cast<int32_t>(attachment.src_alpha_blend_factor));
            h ^= int_hasher(static_cast<int32_t>(attachment.dst_alpha_blend_factor));
            h ^= int_hasher(static_cast<int32_t>(attachment.alpha_blend_op));
            h ^= int_hasher(static_cast<int32_t>(attachment.color_write_mask));
        }

        h ^= float_hasher(desc.color_blend.blend_constants.r);
        h ^= float_hasher(desc.color_blend.blend_constants.g);
        h ^= float_hasher(desc.color_blend.blend_constants.b);
        h ^= float_hasher(desc.color_blend.blend_constants.a);
    }

    return h;
}

size_t PipelineCache::hash(const ComputePipelineDescription& desc) const
{
    size_t h = 0;

    // Shader
    h ^= std::hash<Shader*>()(desc.compute_shader.get());

    return h;
}

size_t PipelineCache::hash(const RayTracingPipelineDescription& desc) const
{
    std::hash<Shader*> shader_hasher;
    std::hash<uint32_t> uint_hasher;

    size_t h = 0;

    // Shaders
    h ^= shader_hasher(desc.raygen_shader.get());
    for (const auto& shader : desc.miss_shaders)
        h ^= shader_hasher(shader.get());
    for (const auto& shader : desc.closest_hit_shaders)
        h ^= shader_hasher(shader.get());

    // Configuration
    h ^= uint_hasher(desc.max_ray_recursion_depth);

    return h;
}

} // namespace Mizu