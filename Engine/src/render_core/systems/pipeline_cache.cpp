#include "pipeline_cache.h"

#include "render_core/rhi/framebuffer.h"

#include "utility/assert.h"

namespace Mizu
{

std::shared_ptr<GraphicsPipeline> PipelineCache::get_pipeline(const GraphicsPipeline::Description& desc)
{
    const size_t h = hash(desc);

    const auto it = m_graphics_cache.find(h);
    if (it != m_graphics_cache.end())
    {
        return it->second;
    }

    const auto pipeline = GraphicsPipeline::create(desc);
    MIZU_ASSERT(pipeline != nullptr, "Failed to create GraphicsPipeline");

    return m_graphics_cache.insert({h, pipeline}).first->second;
}

std::shared_ptr<ComputePipeline> PipelineCache::get_pipeline(const ComputePipeline::Description& desc)
{
    const size_t h = hash(desc);

    const auto it = m_compute_cache.find(h);
    if (it != m_compute_cache.end())
    {
        return it->second;
    }

    const auto pipeline = ComputePipeline::create(desc);
    MIZU_ASSERT(pipeline != nullptr, "Failed to create ComputePipeline");

    return m_compute_cache.insert({h, pipeline}).first->second;
}

std::shared_ptr<RayTracingPipeline> PipelineCache::get_pipeline(const RayTracingPipeline::Description& desc)
{
    const size_t h = hash(desc);

    const auto it = m_ray_tracing_cache.find(h);
    if (it != m_ray_tracing_cache.end())
    {
        return it->second;
    }

    const auto pipeline = RayTracingPipeline::create(desc);
    MIZU_ASSERT(pipeline != nullptr, "Failed to create RayTracingPipeline");

    return m_ray_tracing_cache.insert({h, pipeline}).first->second;
}

size_t PipelineCache::hash(const GraphicsPipeline::Description& desc) const
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
        for (const Framebuffer::Attachment& attachment : desc.target_framebuffer->get_attachments())
        {
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
        h ^= bool_hasher(desc.color_blend.logic_op_enable);
        h ^= float_hasher(desc.color_blend.blend_constants.r);
        h ^= float_hasher(desc.color_blend.blend_constants.g);
        h ^= float_hasher(desc.color_blend.blend_constants.b);
        h ^= float_hasher(desc.color_blend.blend_constants.a);
    }

    return h;
}

size_t PipelineCache::hash(const ComputePipeline::Description& desc) const
{
    size_t h = 0;

    // Shader
    h ^= std::hash<Shader*>()(desc.shader.get());

    return h;
}

size_t PipelineCache::hash(const RayTracingPipeline::Description& desc) const
{
    std::hash<Shader*> shader_hasher;
    std::hash<uint32_t> uint_hasher;

    size_t h = 0;

    // Shaders
    h ^= shader_hasher(desc.raygen_shader.get());
    h ^= shader_hasher(desc.miss_shader.get());
    h ^= shader_hasher(desc.closest_hit_shader.get());

    // Configuration
    h ^= uint_hasher(desc.max_ray_recursion_depth);

    return h;
}

} // namespace Mizu