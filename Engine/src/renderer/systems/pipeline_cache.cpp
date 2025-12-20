#include "pipeline_cache.h"

#include "base/debug/assert.h"
#include "base/utils/hash.h"

#include "renderer/renderer.h"
#include "renderer/shader/shader_declaration.h"

namespace Mizu
{

PipelineCache& PipelineCache::get()
{
    static PipelineCache instance;
    return instance;
}

void PipelineCache::reset()
{
    m_cache.clear();
}

void PipelineCache::insert(size_t hash, std::shared_ptr<Pipeline> pipeline)
{
    MIZU_ASSERT(!contains(hash), "Pipeline with hash {} already exists");
    m_cache.emplace(hash, std::move(pipeline));
}

std::shared_ptr<Pipeline> PipelineCache::get(size_t hash) const
{
    MIZU_ASSERT(contains(hash), "Pipeline with hash {} does not exist");
    return m_cache.find(hash)->second;
}

bool PipelineCache::contains(size_t hash) const
{
    return m_cache.find(hash) != m_cache.end();
}

size_t PipelineCache::get_graphics_pipeline_hash(
    size_t vertex_hash,
    size_t fragment_hash,
    const RasterizationState& raster,
    const DepthStencilState& depth_stencil,
    const ColorBlendState& color_blend,
    const Framebuffer& framebuffer)
{
    size_t h = 0;

    // Shaders
    hash_combine(h, vertex_hash);
    hash_combine(h, fragment_hash);

    // Rasterization state
    {
        hash_combine(h, raster.rasterizer_discard);
        hash_combine(h, raster.polygon_mode);
        hash_combine(h, raster.cull_mode);
        hash_combine(h, raster.front_face);

        hash_combine(h, raster.depth_bias.enabled);
        hash_combine(h, raster.depth_bias.constant_factor);
        hash_combine(h, raster.depth_bias.clamp);
        hash_combine(h, raster.depth_bias.slope_factor);
    }

    // Depth stencil state
    {
        hash_combine(h, depth_stencil.depth_test);
        hash_combine(h, depth_stencil.depth_write);
        hash_combine(h, depth_stencil.depth_compare_op);

        hash_combine(h, depth_stencil.depth_bounds_test);
        hash_combine(h, depth_stencil.min_depth_bounds);
        hash_combine(h, depth_stencil.max_depth_bounds);

        hash_combine(h, depth_stencil.stencil_test);
    }

    // Color blend state
    {
        hash_combine(h, color_blend.method);
        hash_combine(h, color_blend.logic_op);

        for (const ColorBlendState::AttachmentState& attachment : color_blend.attachments)
        {
            hash_combine(h, attachment.blend_enabled);
            hash_combine(h, attachment.src_color_blend_factor);
            hash_combine(h, attachment.dst_color_blend_factor);
            hash_combine(h, attachment.color_blend_op);
            hash_combine(h, attachment.src_alpha_blend_factor);
            hash_combine(h, attachment.dst_alpha_blend_factor);
            hash_combine(h, attachment.alpha_blend_op);
            hash_combine(h, attachment.color_write_mask);
        }

        hash_combine(h, color_blend.blend_constants.r);
        hash_combine(h, color_blend.blend_constants.g);
        hash_combine(h, color_blend.blend_constants.b);
        hash_combine(h, color_blend.blend_constants.a);
    }

    // Framebuffer
    {
        const auto hash_attachment = [](const FramebufferAttachment& attachment) -> size_t {
            return hash_compute(
                attachment.initial_state,
                attachment.final_state,
                attachment.load_operation,
                attachment.store_operation);
        };

        for (const FramebufferAttachment& attachment : framebuffer.get_color_attachments())
        {
            h ^= hash_attachment(attachment);
        }

        const std::optional<const FramebufferAttachment>& depth_stencil_attachment_opt =
            framebuffer.get_depth_stencil_attachment();
        if (depth_stencil_attachment_opt.has_value())
        {
            const FramebufferAttachment& attachment = *depth_stencil_attachment_opt;
            h ^= hash_attachment(attachment);
        }
    }

    return h;
}

size_t PipelineCache::get_compute_pipeline_hash(size_t compute_hash)
{
    return compute_hash;
}

size_t PipelineCache::get_ray_tracing_pipeline_hash(
    size_t raygen_hash,
    size_t miss_hash,
    size_t closest_hit_hash,
    uint32_t max_ray_recursion_depth)
{
    return hash_compute(raygen_hash, miss_hash, closest_hit_hash, max_ray_recursion_depth);
}

static constexpr size_t MAX_VERTEX_INPUTS = 10;
static constexpr size_t MAX_LAYOUT_BINDINGS = 30;

struct PipelineLayoutBuilder
{
  public:
    void add(const SlangReflection& reflection, ShaderType stage)
    {
        for (const ShaderResource& resource : reflection.get_parameters())
        {
            const size_t hash = hash_compute(resource.binding_info.set, resource.binding_info.binding, resource.type);

            const auto it = m_hash_to_layout_pos.find(hash);
            if (it != m_hash_to_layout_pos.end())
            {
                const size_t idx = it->second;
                layout_info[idx].stage |= stage;

                continue;
            }

            switch (resource.type)
            {
            case ShaderResourceType::TextureSrv:
                layout_info.push_back(DescriptorBindingInfo::TextureSrv(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::TextureUav:
                layout_info.push_back(DescriptorBindingInfo::TextureUav(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::StructuredBufferSrv:
                layout_info.push_back(DescriptorBindingInfo::StructuredBufferSrv(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::StructuredBufferUav:
                layout_info.push_back(DescriptorBindingInfo::StructuredBufferUav(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::ByteAddressBufferSrv:
                layout_info.push_back(DescriptorBindingInfo::ByteAddressBufferSrv(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::ByteAddressBufferUav:
                layout_info.push_back(DescriptorBindingInfo::ByteAddressBufferUav(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::ConstantBuffer:
                layout_info.push_back(DescriptorBindingInfo::ConstantBuffer(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::AccelerationStructure:
                layout_info.push_back(DescriptorBindingInfo::AccelerationStructure(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::SamplerState:
                layout_info.push_back(DescriptorBindingInfo::SamplerState(resource.binding_info, 1, stage));
                break;
            case ShaderResourceType::PushConstant:
                MIZU_UNREACHABLE("Invalid shader resource type")
                break;
            }

            m_hash_to_layout_pos.emplace(hash, layout_info.size() - 1);
        }

        for (const ShaderPushConstant& constant : reflection.get_constants())
        {
            const size_t hash = hash_compute(
                constant.binding_info.set,
                constant.binding_info.binding,
                ShaderResourceType::PushConstant,
                constant.size);

            const auto it = m_hash_to_layout_pos.find(hash);
            if (it != m_hash_to_layout_pos.end())
            {
                const size_t idx = it->second;
                layout_info[idx].stage |= stage;

                continue;
            }

            layout_info.push_back(DescriptorBindingInfo::PushConstant(constant.size, stage));

            m_hash_to_layout_pos.emplace(hash, layout_info.size() - 1);
        }
    }

    inplace_vector<DescriptorBindingInfo, MAX_LAYOUT_BINDINGS> layout_info{};

  private:
    std::unordered_map<size_t, size_t> m_hash_to_layout_pos;
};

static size_t get_shader_instance_hash(const ShaderInstance& instance)
{
    return ShaderManager::get_shader_hash(
        instance.virtual_path, instance.entry_point, instance.type, instance.environment);
}

static const SlangReflection& get_shader_instance_reflection(const ShaderInstance& instance)
{
    return ShaderManager::get().get_reflection(
        instance.virtual_path, instance.entry_point, instance.type, instance.environment);
}

static std::shared_ptr<Shader> get_shader(const ShaderInstance& instance)
{
    return ShaderManager::get().get_shader(
        instance.virtual_path, instance.entry_point, instance.type, instance.environment);
}

std::shared_ptr<Pipeline> get_graphics_pipeline(
    const ShaderDeclaration& vertex,
    const ShaderDeclaration& fragment,
    const RasterizationState& raster,
    const DepthStencilState& depth_stencil,
    const ColorBlendState& color_blend,
    std::shared_ptr<Framebuffer> framebuffer)
{
    return get_graphics_pipeline(
        vertex.get_instance(), fragment.get_instance(), raster, depth_stencil, color_blend, framebuffer);
}

std::shared_ptr<Pipeline> get_graphics_pipeline(
    const ShaderInstance& vertex,
    const ShaderInstance& fragment,
    const RasterizationState& raster,
    const DepthStencilState& depth_stencil,
    const ColorBlendState& color_blend,
    std::shared_ptr<Framebuffer> framebuffer)
{
    MIZU_ASSERT(vertex.type == ShaderType::Vertex, "Vertex shader must be ShaderType::Vertex");
    MIZU_ASSERT(fragment.type == ShaderType::Fragment, "Fragment shader must be ShaderType::Fragment");

    const size_t vertex_hash = get_shader_instance_hash(vertex);
    const size_t fragment_hash = get_shader_instance_hash(fragment);

    const size_t pipeline_hash = PipelineCache::get_graphics_pipeline_hash(
        vertex_hash, fragment_hash, raster, depth_stencil, color_blend, *framebuffer);

    PipelineCache& cache = PipelineCache::get();
    if (cache.contains(pipeline_hash))
    {
        return cache.get(pipeline_hash);
    }

    const SlangReflection& vertex_reflection = get_shader_instance_reflection(vertex);
    const SlangReflection& fragment_reflection = get_shader_instance_reflection(fragment);

    inplace_vector<ShaderInputOutput, MAX_VERTEX_INPUTS> vertex_inputs;
    for (const ShaderInputOutput& input : vertex_reflection.get_inputs())
    {
        vertex_inputs.push_back(input);
    }

    PipelineLayoutBuilder builder{};
    builder.add(vertex_reflection, ShaderType::Vertex);
    builder.add(fragment_reflection, ShaderType::Fragment);

    GraphicsPipelineDescription desc{};
    desc.vertex_shader = get_shader(vertex);
    desc.fragment_shader = get_shader(fragment);
    desc.rasterization = raster;
    desc.depth_stencil = depth_stencil;
    desc.color_blend = color_blend;
    desc.vertex_inputs = std::span(vertex_inputs.data(), vertex_inputs.size());
    desc.layout = std::span(builder.layout_info.data(), builder.layout_info.size());
    desc.target_framebuffer = framebuffer;

    const auto pipeline = g_render_device->create_pipeline(desc);
    cache.insert(pipeline_hash, pipeline);

    return pipeline;
}

std::shared_ptr<Pipeline> get_compute_pipeline(const ShaderDeclaration& compute_shader)
{
    return get_compute_pipeline(compute_shader.get_instance());
}

std::shared_ptr<Pipeline> get_compute_pipeline(const ShaderInstance& compute)
{
    MIZU_ASSERT(compute.type == ShaderType::Compute, "Compute shader must be ShaderType::Compute");

    const size_t pipeline_hash = PipelineCache::get_compute_pipeline_hash(get_shader_instance_hash(compute));

    PipelineCache& cache = PipelineCache::get();
    if (cache.contains(pipeline_hash))
    {
        return cache.get(pipeline_hash);
    }

    const SlangReflection& compute_reflection = get_shader_instance_reflection(compute);

    PipelineLayoutBuilder builder{};
    builder.add(compute_reflection, ShaderType::Compute);

    ComputePipelineDescription desc{};
    desc.compute_shader = get_shader(compute);
    desc.layout = std::span(builder.layout_info.data(), builder.layout_info.size());

    const auto pipeline = g_render_device->create_pipeline(desc);
    cache.insert(pipeline_hash, pipeline);

    return pipeline;
}

// std::shared_ptr<Pipeline> get_ray_tracing_pipeline(
//     const ShaderDeclaration& raygen,
//     const RtxShaderDeclarations& miss,
//     const RtxShaderDeclarations& closest_hit,
//     uint32_t max_ray_recursion_depth)
//{
//     RtxShaderInstances miss_instances;
//     RtxShaderInstances closest_hit_instances;
//
//     for (const ShaderDeclaration& miss_declaration : miss)
//     {
//         miss_instances.push_back(miss_declaration.get_instance());
//     }
//
//     for (const ShaderDeclaration& closest_hit_declaration : closest_hit)
//     {
//         miss_instances.push_back(closest_hit_declaration.get_instance());
//     }
//
//     return get_ray_tracing_pipeline(
//         raygen.get_instance(), miss_instances, closest_hit_instances, max_ray_recursion_depth);
// }

std::shared_ptr<Pipeline> get_ray_tracing_pipeline(
    const ShaderInstance& raygen,
    const RtxShaderInstances& miss,
    const RtxShaderInstances& closest_hit,
    uint32_t max_ray_recursion_depth)
{
#if MIZU_DEBUG
    MIZU_ASSERT(raygen.type == ShaderType::RtxRaygen, "Raygen shader must be ShaderType::RtxRaygen");

    for (const ShaderInstance& m : miss)
    {
        MIZU_ASSERT(m.type == ShaderType::RtxMiss, "Miss shader must be ShaderType::RtxMiss");
    }

    for (const ShaderInstance& ch : closest_hit)
    {
        MIZU_ASSERT(ch.type == ShaderType::RtxClosestHit, "Closest hit shader must be ShaderType::RtxClosestHit");
    }
#endif

    const size_t raygen_hash = get_shader_instance_hash(raygen);

    size_t miss_hash = 0;
    for (const ShaderInstance& m : miss)
    {
        hash_combine(miss_hash, get_shader_instance_hash(m));
    }

    size_t closest_hit_hash = 0;
    for (const ShaderInstance& ch : closest_hit)
    {
        hash_combine(closest_hit_hash, get_shader_instance_hash(ch));
    }

    const size_t pipeline_hash =
        PipelineCache::get_ray_tracing_pipeline_hash(raygen_hash, miss_hash, closest_hit_hash, max_ray_recursion_depth);

    PipelineCache& cache = PipelineCache::get();
    if (cache.contains(pipeline_hash))
    {
        return cache.get(pipeline_hash);
    }

    PipelineLayoutBuilder builder{};

    {
        const SlangReflection& raygen_reflection = get_shader_instance_reflection(raygen);
        builder.add(raygen_reflection, ShaderType::RtxRaygen);
    }

    for (const ShaderInstance& m : miss)
    {
        const SlangReflection& miss_reflection = get_shader_instance_reflection(m);
        builder.add(miss_reflection, ShaderType::RtxMiss);
    }

    for (const ShaderInstance& ch : closest_hit)
    {
        const SlangReflection& closest_hit_reflection = get_shader_instance_reflection(ch);
        builder.add(closest_hit_reflection, ShaderType::RtxClosestHit);
    }

    RayTracingPipelineDescription desc{};
    desc.raygen_shader = get_shader(raygen);

    for (const ShaderInstance& m : miss)
    {
        desc.miss_shaders.push_back(get_shader(m));
    }

    for (const ShaderInstance& ch : closest_hit)
    {
        desc.closest_hit_shaders.push_back(get_shader(ch));
    }

    desc.layout = std::span(builder.layout_info.data(), builder.layout_info.size());
    desc.max_ray_recursion_depth = max_ray_recursion_depth;

    const auto pipeline = g_render_device->create_pipeline(desc);
    cache.insert(pipeline_hash, pipeline);

    return pipeline;
}

} // namespace Mizu