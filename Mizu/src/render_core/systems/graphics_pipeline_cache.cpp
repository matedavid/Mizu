#include "graphics_pipeline_cache.h"

namespace Mizu
{

std::shared_ptr<GraphicsPipeline> GraphicsPipelineCache::get_pipeline(const GraphicsPipeline::Description& desc)
{
    const size_t h = hash(desc);

    const auto it = m_cache.find(h);
    if (it != m_cache.end())
    {
        return it->second;
    }

    const auto pipeline = GraphicsPipeline::create(desc);
    return m_cache.insert({h, pipeline}).first->second;
}

size_t GraphicsPipelineCache::hash(const GraphicsPipeline::Description& desc) const
{
    std::hash<bool> bool_hasher;
    std::hash<int32_t> int_hasher;
    std::hash<float> float_hasher;

    size_t h = 0;

    // Shader
    h ^= std::hash<GraphicsShader*>()(desc.shader.get());

    // Target framebuffer
    // h ^= std::hash<Framebuffer*>()(desc.target_framebuffer.get());

    // Rasterization state
    {
        h ^= bool_hasher(desc.rasterization.rasterizer_discard);
        h ^= int_hasher(static_cast<int32_t>(desc.rasterization.polygon_mode));
        h ^= int_hasher(static_cast<int32_t>(desc.rasterization.cull_mode));
        h ^= int_hasher(static_cast<int32_t>(desc.rasterization.front_face));

        h ^= bool_hasher(desc.rasterization.depth_bias.enabled);
        h ^= int_hasher(desc.rasterization.depth_bias.constant_factor);
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

} // namespace Mizu