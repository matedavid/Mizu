#include "scene/draw_list_system_internal.h"

#include <algorithm>
#include <glm/glm.hpp>
#include <limits>

#include "asset/asset_registry.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "base/math/aabb.h"
#include "core/game_context.h"
#include "core/runtime.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render.pipeline/scene_shaders.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/runtime/renderer.h"
#include "render/runtime/renderer_settings.h"
#include "render/systems/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"
#include "resources/gpu_pools.h"
#include "scene/draw_class_registry.h"
#include "scene/scene_system.h"

namespace Mizu
{

namespace
{

struct DrawIndexPushConstant
{
    uint32_t draw_index;
};

#if MIZU_DEBUG

bool framebuffer_info_equal(const FramebufferInfo& a, const FramebufferInfo& b)
{
    if (a.depth_stencil_attachment != b.depth_stencil_attachment)
        return false;

    if (a.color_attachments.size() != b.color_attachments.size())
        return false;

    for (size_t i = 0; i < a.color_attachments.size(); ++i)
    {
        if (a.color_attachments[i] != b.color_attachments[i])
            return false;
    }

    return true;
}

#endif

size_t get_raster_state_hash(const RasterState& state, const FramebufferInfo& framebuffer_info)
{
    // TODO: Using 0 for shader handles here because we want the hash to only contain the raster state.
    return PipelineCache::get_graphics_pipeline_hash(
        0, 0, state.rasterization, state.depth_stencil, state.color_blend, framebuffer_info);
}

} // namespace

DrawListSystem::DrawListSystem(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool)
    : m_scene_system(scene_system)
    , m_gpu_mesh_pool(gpu_mesh_pool)
{
    m_gpu_driven_rendering_enabled = get_setting<RendererSettings>().gpu_driven_rendering_enabled;
}

void DrawListSystem::reset()
{
    MIZU_PROFILE_SCOPED;

    for (VisibilityEntry& visibility : m_visibilities)
    {
        visibility.visible.clear();
        visibility.ranges.clear();
        visibility.draw_lists.clear();
        visibility.list_begin = 0;
    }

    for (DrawListEntry& list : m_draw_lists)
    {
        list.raster_pass = nullptr;
        list.class_to_bucket.clear();
        list.buckets.clear();
        list.elements.clear();
        list.draw_data.clear();
        list.draw_data_allocation = FrameAllocation{};
        list.bucket_map_offset = 0;
    }

    m_num_visibilities = 0;
    m_num_draw_lists = 0;

    m_visibility_map.clear();
    m_draw_list_map.clear();

    m_gpu_resources = GpuResources{};
    m_gpu_driven_rendering_enabled = get_setting<RendererSettings>().gpu_driven_rendering_enabled;
}

DrawListHandle DrawListSystem::create_draw_list(const DrawListRequest& request)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(request.raster_pass != nullptr, "Can't create draw list without a DrawListRasterPass");
    MIZU_ASSERT(request.view_count > 0, "View count must be greater than 0");

    if (m_gpu_driven_rendering_enabled && m_gpu_resources.valid)
    {
        request.pass_builder.indirect_argument(m_gpu_resources.indirect_commands_buffer);
        request.pass_builder.indirect_argument(m_gpu_resources.count_buffer);
        request.pass_builder.read(m_gpu_resources.draw_data_buffer);
    }

    const uint32_t visibility_idx = get_visibility_id(VisibilityDesc::create(request.frustum, request.frustum_mask));

    const DrawFilter filter = request.raster_pass->filter();

    const DrawListKey key{
        .visibility_idx = visibility_idx,
        .view_count = request.view_count,
        .filter_required = filter.required,
        .filter_excluded = filter.excluded,
        .raster_state_hash = get_raster_state_hash(request.state, request.targets),
    };

    const size_t hash = key.hash();

    const auto range = m_draw_list_map.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (m_draw_lists[it->second].key == key)
            return DrawListHandle{.index = it->second};
    }

    const uint32_t index = m_num_draw_lists;
    if (index == m_draw_lists.size())
    {
        m_draw_lists.emplace_back();
    }

    DrawListEntry& list = m_draw_lists[index];
    list.key = key;
    list.raster_pass = request.raster_pass;
    list.state = request.state;
    list.targets = request.targets;
    list.filter = filter;
    list.view_count = request.view_count;
    list.visibility_idx = visibility_idx;

    m_num_draw_lists += 1;
    m_draw_list_map.insert({hash, index});

    return DrawListHandle{.index = index};
}

void DrawListSystem::add_passes(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    if (m_gpu_driven_rendering_enabled)
    {
        gpu_add_passes(builder, frame_allocator);
    }
}

void DrawListSystem::finalize(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    MIZU_PROFILE_SCOPED;

    if (m_num_draw_lists == 0)
        return;

    const DrawClassRegistry& registry = m_scene_system.get_draw_class_registry();
    const uint32_t num_draw_class_entries = registry.num_entries();
    const std::span<const DrawClassId> live_classes = registry.live_classes();

    uint32_t num_bucket_slots = 0;
    uint32_t total_capacity = 0;

    for (uint32_t list_idx = 0; list_idx < m_num_draw_lists; ++list_idx)
    {
        DrawListEntry& list = m_draw_lists[list_idx];

        list.class_to_bucket.assign(num_draw_class_entries, INVALID_BUCKET_INDEX);
        list.buckets.clear();

        for (const DrawClassId class_id : live_classes)
        {
            const std::optional<RasterShaders> shaders = list.raster_pass->select(registry.get_desc(class_id));
            if (!shaders.has_value())
                continue;

            uint32_t bucket_idx = INVALID_BUCKET_INDEX;
            for (uint32_t i = 0; i < list.buckets.size(); ++i)
            {
                if (list.buckets[i].shaders == *shaders)
                {
                    bucket_idx = i;
                    break;
                }
            }

            if (bucket_idx == INVALID_BUCKET_INDEX)
            {
                bucket_idx = static_cast<uint32_t>(list.buckets.size());
                list.buckets.push_back(DrawBucket{.shaders = *shaders});
            }

            list.class_to_bucket[class_id] = bucket_idx;

            // One command per drawable regardless of the view count and instancing
            list.buckets[bucket_idx].capacity += registry.get_live_count(class_id);
        }

        if (!m_gpu_driven_rendering_enabled)
            continue;

        list.bucket_map_offset = list_idx * num_draw_class_entries;

        for (DrawBucket& bucket : list.buckets)
        {
            bucket.slot = num_bucket_slots;
            bucket.region_base = total_capacity;

            num_bucket_slots += 1;
            total_capacity += bucket.capacity;
        }
    }

    if (!m_gpu_driven_rendering_enabled || !m_gpu_resources.valid)
        return;

    m_gpu_resources.num_bucket_slots = num_bucket_slots;
    m_gpu_resources.total_capacity = total_capacity;

    if (num_bucket_slots == 0 || total_capacity == 0)
    {
        m_gpu_resources.valid = false;
        return;
    }

    builder.set_deferred_buffer_size(
        m_gpu_resources.indirect_commands_buffer, sizeof(DrawIndexedIndirectCommand) * total_capacity);
    builder.set_deferred_buffer_size(m_gpu_resources.count_buffer, sizeof(uint32_t) * num_bucket_slots);
    builder.set_deferred_buffer_size(m_gpu_resources.draw_data_buffer, sizeof(GpuDrawData) * total_capacity);

    gpu_build_tables(frame_allocator);
}

void DrawListSystem::prepare(FrameLinearAllocator& frame_allocator)
{
    MIZU_PROFILE_SCOPED;

    if (m_num_draw_lists == 0)
        return;

    resolve_pipelines();

    if (m_gpu_driven_rendering_enabled)
        return;

    PendingBatch visibility_batch = g_job_system->schedule_batch();
    for (uint32_t i = 0; i < m_num_visibilities; ++i)
    {
        visibility_batch.add(&DrawListSystem::cpu_visibility_job, this, i);
    }

    const JobHandle visibilities_handle = visibility_batch.submit();
    g_job_system->wait_for(visibilities_handle);

    PendingBatch list_batch = g_job_system->schedule_batch();
    for (uint32_t i = 0; i < m_num_draw_lists; ++i)
    {
        list_batch.add(&DrawListSystem::cpu_build_list_job, this, i);
    }

    const JobHandle build_lists_handle = list_batch.submit();
    g_job_system->wait_for(build_lists_handle);

    cpu_upload_draw_data(frame_allocator);
}

void DrawListSystem::dispatch_draw_list(
    CommandBuffer& command,
    DrawListHandle handle,
    const DrawListRasterPassInfo& info)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(handle.is_valid(), "Invalid draw list handle");
    MIZU_ASSERT(handle.index < m_num_draw_lists, "Draw list handle index is out of range");

    const DrawListEntry& list = m_draw_lists[handle.index];

#if MIZU_DEBUG
    MIZU_ASSERT(
        framebuffer_info_equal(info.framebuffer_info, list.targets),
        "Draw list '{}' was created with targets that do not match the render pass it is dispatched into",
        list.raster_pass->name());
#endif

    if (list.buckets.empty())
        return;

    const bool gpu_driven = m_gpu_driven_rendering_enabled;

    if (gpu_driven && !m_gpu_resources.valid)
        return;

    if (!gpu_driven && list.elements.empty())
        return;

    command.bind_vertex_buffer(*m_gpu_mesh_pool.get_vertex_buffer());
    command.bind_index_buffer(*m_gpu_mesh_pool.get_index_buffer());

    const std::shared_ptr<DescriptorSet> draw_list_descriptor_set = create_draw_list_descriptor_set(list);

    // TODO: By default setting the DrawListSystem resources at set 0, this could be problematic as it's not
    // clear to the user that we're doing this.
    constexpr uint32_t system_set = 0;

    for (const DrawBucket& bucket : list.buckets)
    {
        if (bucket.pipeline == nullptr)
            continue;

        if (gpu_driven && bucket.capacity == 0)
            continue;

        if (!gpu_driven && bucket.element_count == 0)
            continue;

        command.bind_pipeline(bucket.pipeline);
        command.bind_descriptor_set(*draw_list_descriptor_set, system_set);

        for (uint32_t set = 0; set < MAX_DESCRIPTOR_SET_COUNT; ++set)
        {
            const std::shared_ptr<DescriptorSet>& descriptor_set = info.bindings.descriptor_sets[set];
            if (descriptor_set != nullptr)
            {
                MIZU_ASSERT(set != system_set, "Descriptor set {} is reserved by the draw list system", set);
                command.bind_descriptor_set(*descriptor_set, set);
            }
        }

        if (gpu_driven)
        {
            gpu_dispatch_bucket(command, bucket);
        }
        else
        {
            cpu_dispatch_bucket(command, list, bucket);
        }
    }
}

uint32_t DrawListSystem::get_visibility_id(const VisibilityDesc& desc)
{
    const size_t hash = desc.hash();

    const auto range = m_visibility_map.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (m_visibilities[it->second].desc == desc)
            return it->second;
    }

    const uint32_t index = m_num_visibilities;
    if (index == m_visibilities.size())
    {
        m_visibilities.emplace_back();
    }

    m_visibilities[index].desc = desc;
    m_num_visibilities += 1;

    m_visibility_map.insert({hash, index});

    return index;
}

void DrawListSystem::resolve_pipelines()
{
    MIZU_PROFILE_SCOPED;

    for (uint32_t list_idx = 0; list_idx < m_num_draw_lists; ++list_idx)
    {
        DrawListEntry& list = m_draw_lists[list_idx];

        for (DrawBucket& bucket : list.buckets)
        {
            bucket.pipeline = get_graphics_pipeline(
                bucket.shaders.vertex,
                bucket.shaders.fragment,
                list.state.rasterization,
                list.state.depth_stencil,
                list.state.color_blend,
                list.targets);
        }
    }
}

//
// Cpu backend
//

static bool filter_accepts(DrawableFlags flags, const DrawFilter& filter)
{
    const uint32_t bits = static_cast<uint32_t>(flags);

    if ((bits & static_cast<uint32_t>(filter.required)) != static_cast<uint32_t>(filter.required))
        return false;

    return (bits & static_cast<uint32_t>(filter.excluded)) == 0;
}

void DrawListSystem::cpu_visibility_job(uint32_t visibility_idx)
{
    MIZU_PROFILE_SCOPED;

    VisibilityEntry& visibility = m_visibilities[visibility_idx];
    const VisibilityDesc& desc = visibility.desc;

    const std::span<const SceneDrawableInfo> drawables = m_scene_system.get_drawables();
    const std::span<const TransformInfo> transforms = m_scene_system.get_transform_infos();

    visibility.visible.clear();
    visibility.visible.reserve(drawables.size());

    for (uint32_t i = 0; i < drawables.size(); ++i)
    {
        const SceneDrawableInfo& drawable = drawables[i];

        if (!drawable.gpu_mesh_record.allocation.handle.is_valid())
        {
            MIZU_LOG_ERROR("Drawable with invalid Gpu mesh allocation handle, skipping.");
            continue;
        }

        if (drawable.material_buffer_offset == std::numeric_limits<uint32_t>::max())
        {
            MIZU_LOG_ERROR("Drawable with invalid Material buffer offset, skipping.");
            continue;
        }

        if (drawable.class_id == INVALID_DRAW_CLASS_ID)
            continue;

        if (desc.has_frustum)
        {
            const AABB& local_aabb = drawable.gpu_mesh_record.metadata.bounding_box;
            const glm::mat4& world_transform = transforms[drawable.transform_slot_index].transform;

            const AABB world_aabb = transform_aabb(local_aabb, world_transform);

            if (!desc.frustum.is_inside_frustum(world_aabb, desc.mask))
                continue;
        }

        visibility.visible.push_back(i);
    }

    std::sort(visibility.visible.begin(), visibility.visible.end(), [&](uint32_t a, uint32_t b) {
        const SceneDrawableInfo& lhs = drawables[a];
        const SceneDrawableInfo& rhs = drawables[b];

        if (lhs.class_id != rhs.class_id)
            return lhs.class_id < rhs.class_id;

        if (lhs.material_buffer_offset != rhs.material_buffer_offset)
            return lhs.material_buffer_offset < rhs.material_buffer_offset;

        if (lhs.gpu_mesh_draw.first_vertex != rhs.gpu_mesh_draw.first_vertex)
            return lhs.gpu_mesh_draw.first_vertex < rhs.gpu_mesh_draw.first_vertex;

        if (lhs.gpu_mesh_draw.first_index != rhs.gpu_mesh_draw.first_index)
            return lhs.gpu_mesh_draw.first_index < rhs.gpu_mesh_draw.first_index;

        return lhs.gpu_mesh_draw.index_count < rhs.gpu_mesh_draw.index_count;
    });

    visibility.ranges.clear();

    // We can do this because we just sorted the drawables by class id, so we can just create ranges of consecutive
    // drawables with the same class id.
    for (uint32_t i = 0; i < visibility.visible.size(); ++i)
    {
        const uint32_t class_id = drawables[visibility.visible[i]].class_id;

        VisibilityEntry::DrawClassRange* back = visibility.ranges.empty() ? nullptr : &visibility.ranges.back();
        if (back != nullptr && back->class_id == class_id)
        {
            back->end = i + 1;
        }
        else
        {
            visibility.ranges.push_back(
                VisibilityEntry::DrawClassRange{
                    .class_id = class_id,
                    .begin = i,
                    .end = i + 1,
                });
        }
    }
}

void DrawListSystem::cpu_build_list_job(uint32_t list_idx)
{
    MIZU_PROFILE_SCOPED;

    DrawListEntry& list = m_draw_lists[list_idx];

    list.elements.clear();
    list.draw_data.clear();

    if (list.buckets.empty())
        return;

    const VisibilityEntry& visibility = m_visibilities[list.visibility_idx];
    const std::span<const SceneDrawableInfo> drawables = m_scene_system.get_drawables();

    list.elements.reserve(visibility.visible.size());
    list.draw_data.reserve(visibility.visible.size());

#if MIZU_DEBUG
    const AssetRegistry& asset_registry = g_game_context->get_asset_registry();
#endif

    for (uint32_t bucket_idx = 0; bucket_idx < list.buckets.size(); ++bucket_idx)
    {
        DrawBucket& bucket = list.buckets[bucket_idx];
        bucket.element_first = static_cast<uint32_t>(list.elements.size());

        uint32_t last_material = std::numeric_limits<uint32_t>::max();
        GpuMeshDrawPayload last_draw{};
        bool has_run = false;

        for (const VisibilityEntry::DrawClassRange& range : visibility.ranges)
        {
            MIZU_ASSERT(range.class_id < list.class_to_bucket.size(), "Drawable class id is out of range");

            if (list.class_to_bucket[range.class_id] != bucket_idx)
                continue;

            for (uint32_t i = range.begin; i < range.end; ++i)
            {
                const SceneDrawableInfo& drawable = drawables[visibility.visible[i]];

                if (!filter_accepts(drawable.flags, list.filter))
                    continue;

                const uint32_t draw_index = static_cast<uint32_t>(list.draw_data.size());
                list.draw_data.push_back(
                    GpuDrawData{
                        .transform_slot = drawable.transform_slot_index,
                        .material_offset = drawable.material_buffer_offset,
                    });

                const bool can_merge = has_run && drawable.material_buffer_offset == last_material
                                       && drawable.gpu_mesh_draw.index_count == last_draw.index_count
                                       && drawable.gpu_mesh_draw.first_index == last_draw.first_index
                                       && drawable.gpu_mesh_draw.first_vertex == last_draw.first_vertex;

                if (can_merge)
                {
                    list.elements.back().instance_count += 1;
                    continue;
                }

#if MIZU_DEBUG
                std::string_view debug_name = asset_registry.get_virtual_path(drawable.mesh_handle);
                if (debug_name.empty())
                    debug_name = "Mesh";
#endif

                list.elements.push_back(
                    DrawElement{
                        .index_count = drawable.gpu_mesh_draw.index_count,
                        .first_index = drawable.gpu_mesh_draw.first_index,
                        .first_vertex = drawable.gpu_mesh_draw.first_vertex,
                        .instance_count = 1,
                        .draw_index = draw_index,
#if MIZU_DEBUG
                        .debug_name = debug_name,
#endif
                    });

                last_material = drawable.material_buffer_offset;
                last_draw = drawable.gpu_mesh_draw;
                has_run = true;
            }
        }

        bucket.element_count = static_cast<uint32_t>(list.elements.size()) - bucket.element_first;
    }
}

void DrawListSystem::cpu_upload_draw_data(FrameLinearAllocator& frame_allocator)
{
    MIZU_PROFILE_SCOPED;

    for (uint32_t list_idx = 0; list_idx < m_num_draw_lists; ++list_idx)
    {
        DrawListEntry& list = m_draw_lists[list_idx];

        if (list.draw_data.empty())
            continue;

        const FrameAllocation allocation = frame_allocator.allocate_structured<GpuDrawData>(list.draw_data.size());
        allocation.upload(std::span<const GpuDrawData>(list.draw_data));

        list.draw_data_allocation = allocation;
    }
}

void DrawListSystem::cpu_dispatch_bucket(CommandBuffer& command, const DrawListEntry& list, const DrawBucket& bucket)
    const
{
#if MIZU_DEBUG
    // TODO: Should probably pass the actual pipeline hash
    const std::string pipeline_debug = "Pipeline: " + std::to_string((uint64_t)bucket.pipeline.get());
    command.begin_gpu_marker(pipeline_debug.c_str());
#endif

    for (uint32_t i = 0; i < bucket.element_count; ++i)
    {
        const DrawElement& element = list.elements[bucket.element_first + i];
        command.push_constant<DrawIndexPushConstant>({
            .draw_index = element.draw_index,
        });

#if MIZU_DEBUG
        command.begin_gpu_marker(element.debug_name);
#endif

        command.draw_indexed(
            element.index_count,
            element.first_index,
            element.first_vertex,
            element.instance_count * list.view_count,
            0);

#if MIZU_DEBUG
        command.end_gpu_marker();
#endif
    }

#if MIZU_DEBUG
    command.end_gpu_marker();
#endif
}

//
// Gpu backend
//

void DrawListSystem::gpu_add_passes(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    MIZU_PROFILE_SCOPED;

    const std::span<const SceneDrawableInfo> drawables = m_scene_system.get_drawables();
    if (drawables.empty())
        return;

    std::vector<GpuDrawableInstance> instances(drawables.size());
    for (size_t i = 0; i < drawables.size(); ++i)
    {
        const SceneDrawableInfo& drawable = drawables[i];

        MIZU_ASSERT(drawable.class_id < (1u << 16), "Draw class id does not fit in the gpu instance record");

        instances[i] = GpuDrawableInstance{
            .aabb_min = drawable.gpu_mesh_record.metadata.bounding_box.min(),
            .transform_slot = drawable.transform_slot_index,
            .aabb_max = drawable.gpu_mesh_record.metadata.bounding_box.max(),
            .material_offset = drawable.material_buffer_offset,
            .index_count = drawable.gpu_mesh_draw.index_count,
            .first_index = drawable.gpu_mesh_draw.first_index,
            .first_vertex = drawable.gpu_mesh_draw.first_vertex,
            .class_and_flags = drawable.class_id | (static_cast<uint32_t>(drawable.flags) << 16u),
        };
    }

    const FrameAllocation instances_allocation =
        frame_allocator.allocate_structured<GpuDrawableInstance>(instances.size());
    instances_allocation.upload(std::span<const GpuDrawableInstance>(instances));

    BufferDescription indirect_commands_desc{};
    indirect_commands_desc.size = sizeof(DrawIndexedIndirectCommand);
    indirect_commands_desc.stride = sizeof(DrawIndexedIndirectCommand);
    indirect_commands_desc.usage = BufferUsageBits::UnorderedAccess | BufferUsageBits::TransferDst
                                   | BufferUsageBits::IndirectBuffer | BufferUsageBits::ShaderResource;
    indirect_commands_desc.name = "DrawListSystem::IndirectCommandsBuffer";

    BufferDescription count_desc{};
    count_desc.size = sizeof(uint32_t);
    count_desc.stride = sizeof(uint32_t);
    count_desc.usage = BufferUsageBits::UnorderedAccess | BufferUsageBits::TransferDst | BufferUsageBits::IndirectBuffer
                       | BufferUsageBits::ShaderResource;
    count_desc.name = "DrawListSystem::IndirectCountBuffer";

    BufferDescription draw_data_desc{};
    draw_data_desc.size = sizeof(GpuDrawData);
    draw_data_desc.stride = sizeof(GpuDrawData);
    draw_data_desc.usage = BufferUsageBits::ShaderResource | BufferUsageBits::UnorderedAccess;
    draw_data_desc.name = "DrawListSystem::GpuDrawDataBuffer";

    m_gpu_resources = GpuResources{};
    m_gpu_resources.valid = true;
    m_gpu_resources.num_drawables = static_cast<uint32_t>(drawables.size());
    m_gpu_resources.instances = instances_allocation;
    m_gpu_resources.indirect_commands_buffer = builder.create_buffer(indirect_commands_desc);
    m_gpu_resources.count_buffer = builder.create_buffer(count_desc);
    m_gpu_resources.draw_data_buffer = builder.create_buffer(draw_data_desc);

    gpu_add_culling_and_generate_pass(builder);
}

void DrawListSystem::gpu_add_culling_and_generate_pass(RenderGraphBuilder& builder)
{
    struct ClearPassData
    {
        RenderGraphResource count_buffer;
    };

    const RenderGraphResource count_buffer = m_gpu_resources.count_buffer;
    const RenderGraphResource indirect_commands_buffer = m_gpu_resources.indirect_commands_buffer;
    const RenderGraphResource draw_data_buffer = m_gpu_resources.draw_data_buffer;

    builder.add_pass<ClearPassData>(
        "DrawListSystem::ClearCounts",
        [&](RenderGraphPassBuilder& pass, ClearPassData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);
            data.count_buffer = pass.write(count_buffer);
        },
        [](CommandBuffer& command, const ClearPassData& data, const RenderGraphPassResources& resources) {
            command.fill_buffer(*resources.get_buffer(data.count_buffer), 0);
        });

    struct CullPassData
    {
        RenderGraphResource count_buffer;
        RenderGraphResource command_buffer;
        RenderGraphResource draw_data_buffer;
    };

    builder.add_pass<CullPassData>(
        "DrawListSystem::CullAndGenerate",
        [&](RenderGraphPassBuilder& pass, CullPassData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.count_buffer = pass.write(count_buffer);
            data.command_buffer = pass.write(indirect_commands_buffer);
            data.draw_data_buffer = pass.write(draw_data_buffer);
        },
        [this](CommandBuffer& command, const CullPassData& data, const RenderGraphPassResources& resources) {
            if (!m_gpu_resources.valid)
                return;

            const auto count_buffer_res = resources.get_buffer(data.count_buffer);
            const auto indirect_commands_buffer_res = resources.get_buffer(data.command_buffer);
            const auto draw_data_buffer_res = resources.get_buffer(data.draw_data_buffer);

            m_gpu_resources.resolved_count_buffer = count_buffer_res.get();
            m_gpu_resources.resolved_indirect_commands_buffer = indirect_commands_buffer_res.get();
            m_gpu_resources.resolved_draw_data_buffer = draw_data_buffer_res.get();

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(Layout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute) // g_instances
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Compute) // g_transform_info
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::Compute) // g_visibilities
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(3, 1, ShaderType::Compute) // g_visibility_lists
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(4, 1, ShaderType::Compute) // g_draw_lists
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(5, 1, ShaderType::Compute) // g_bucket_map
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(6, 1, ShaderType::Compute) // g_buckets
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(0, 1, ShaderType::Compute) // g_counts
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(1, 1, ShaderType::Compute) // g_commands
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(2, 1, ShaderType::Compute) // g_draw_data
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, m_gpu_resources.instances.view),
                WriteDescriptor::StructuredBufferSrv(
                    1, BufferResourceView::create(m_scene_system.get_transform_info_buffer())),
                WriteDescriptor::StructuredBufferSrv(2, m_gpu_resources.visibilities.view),
                WriteDescriptor::StructuredBufferSrv(3, m_gpu_resources.visibility_lists.view),
                WriteDescriptor::StructuredBufferSrv(4, m_gpu_resources.draw_lists.view),
                WriteDescriptor::StructuredBufferSrv(5, m_gpu_resources.bucket_map.view),
                WriteDescriptor::StructuredBufferSrv(6, m_gpu_resources.buckets.view),
                WriteDescriptor::StructuredBufferUav(0, BufferResourceView::create(count_buffer_res)),
                WriteDescriptor::StructuredBufferUav(1, BufferResourceView::create(indirect_commands_buffer_res)),
                WriteDescriptor::StructuredBufferUav(2, BufferResourceView::create(draw_data_buffer_res)),
            };

            const auto descriptor_set =
                g_render_device->allocate_descriptor_set(Layout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.bind_pipeline(get_compute_pipeline(DrawListCullAndGenerateCS{}));
            command.bind_descriptor_set(*descriptor_set, 0);

            const glm::uvec3 group_count = compute_group_count(
                glm::uvec3{m_gpu_resources.num_drawables, 1, 1},
                glm::uvec3{DrawListCullAndGenerateCS::GROUP_SIZE, 1, 1});

            struct PushConstant
            {
                uint32_t visibility_idx;
                uint32_t num_drawables;
            };

            for (uint32_t i = 0; i < m_num_visibilities; ++i)
            {
                if (m_visibilities[i].draw_lists.empty())
                    continue;

                command.push_constant<PushConstant>({
                    .visibility_idx = i,
                    .num_drawables = m_gpu_resources.num_drawables,
                });

                command.dispatch(group_count);
            }
        });
}

void DrawListSystem::gpu_build_tables(FrameLinearAllocator& frame_allocator)
{
    MIZU_PROFILE_SCOPED;

    const DrawClassRegistry& registry = m_scene_system.get_draw_class_registry();
    const uint32_t num_draw_class_entries = registry.num_entries();

    std::vector<uint32_t> visibility_lists{};
    visibility_lists.reserve(m_num_draw_lists);

    for (uint32_t draw_list_idx = 0; draw_list_idx < m_num_draw_lists; ++draw_list_idx)
    {
        const uint32_t visibility_idx = m_draw_lists[draw_list_idx].visibility_idx;
        VisibilityEntry& visibility = m_visibilities[visibility_idx];
        visibility.draw_lists.push_back(draw_list_idx);
    }

    std::vector<GpuVisibilityInfo> visibilities(m_num_visibilities);
    for (uint32_t i = 0; i < m_num_visibilities; ++i)
    {
        const VisibilityEntry& entry = m_visibilities[i];

        GpuVisibilityInfo info{};
        info.draw_list_begin = static_cast<uint32_t>(visibility_lists.size());
        info.draw_list_count = static_cast<uint32_t>(entry.draw_lists.size());

        if (entry.desc.has_frustum)
        {
            info.frustum_mask = entry.desc.mask.to_uint8();

            info.planes[0] = entry.desc.frustum.top.to_vec4();
            info.planes[1] = entry.desc.frustum.bottom.to_vec4();
            info.planes[2] = entry.desc.frustum.left.to_vec4();
            info.planes[3] = entry.desc.frustum.right.to_vec4();
            info.planes[4] = entry.desc.frustum.near.to_vec4();
            info.planes[5] = entry.desc.frustum.far.to_vec4();
        }

        visibilities[i] = info;
        visibility_lists.insert(visibility_lists.end(), entry.draw_lists.begin(), entry.draw_lists.end());
    }

    std::vector<GpuDrawListInfo> draw_lists(m_num_draw_lists);
    std::vector<uint32_t> bucket_map(m_num_draw_lists * num_draw_class_entries, INVALID_BUCKET_SLOT);
    std::vector<GpuBucketInfo> buckets(m_gpu_resources.num_bucket_slots);

    for (uint32_t list_idx = 0; list_idx < m_num_draw_lists; ++list_idx)
    {
        const DrawListEntry& list = m_draw_lists[list_idx];

        draw_lists[list_idx] = GpuDrawListInfo{
            .bucket_map_offset = list.bucket_map_offset,
            .view_count = list.view_count,
            .filter_required = static_cast<uint32_t>(list.filter.required),
            .filter_excluded = static_cast<uint32_t>(list.filter.excluded),
        };

        for (uint32_t class_id = 0; class_id < num_draw_class_entries; ++class_id)
        {
            const uint32_t bucket_idx = list.class_to_bucket[class_id];
            if (bucket_idx == INVALID_BUCKET_INDEX)
                continue;

            bucket_map[list.bucket_map_offset + class_id] = list.buckets[bucket_idx].slot;
        }

        for (const DrawBucket& bucket : list.buckets)
        {
            buckets[bucket.slot] = GpuBucketInfo{
                .region_base = bucket.region_base,
                .capacity = bucket.capacity,
            };
        }
    }

    const auto upload = [&]<typename T>(const std::vector<T>& values) -> FrameAllocation {
        const FrameAllocation allocation = frame_allocator.allocate_structured<T>(values.size());
        allocation.upload(std::span<const T>(values));
        return allocation;
    };

    m_gpu_resources.visibilities = upload(visibilities);
    m_gpu_resources.visibility_lists = upload(visibility_lists);
    m_gpu_resources.draw_lists = upload(draw_lists);
    m_gpu_resources.bucket_map = upload(bucket_map);
    m_gpu_resources.buckets = upload(buckets);
}

void DrawListSystem::gpu_dispatch_bucket(CommandBuffer& command, const DrawBucket& bucket) const
{
    // The shader writes `draw_index` relative to the bucket region, so the base is added
    // here and the vertex shader ends up reading `region_base + draw_index + SV_DrawIndex`.
    command.push_constant<DrawIndexPushConstant>({
        .draw_index = bucket.region_base,
    });

    command.draw_indexed_indirect_count(
        *m_gpu_resources.resolved_indirect_commands_buffer,
        bucket.region_base * sizeof(DrawIndexedIndirectCommand),
        *m_gpu_resources.resolved_count_buffer,
        bucket.slot * sizeof(uint32_t),
        bucket.capacity,
        sizeof(DrawIndexedIndirectCommand));
}

//
// Execution
//

std::shared_ptr<DescriptorSet> DrawListSystem::create_draw_list_descriptor_set(const DrawListEntry& list) const
{
    BufferResourceView draw_data_view{};

    if (m_gpu_driven_rendering_enabled)
    {
        MIZU_ASSERT(m_gpu_resources.resolved_draw_data_buffer != nullptr, "Gpu draw data buffer has not been resolved");
        draw_data_view = BufferResourceView::create(m_gpu_resources.resolved_draw_data_buffer);
    }
    else
    {
        draw_data_view = list.draw_data_allocation.view;
    }

    // clang-format off
    MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(DrawListsSystemLayout)
        MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex) // g_transform_info
        MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Vertex) // g_draw_data
    MIZU_END_DESCRIPTOR_SET_LAYOUT()
    // clang-format on

    const std::array writes = {
        WriteDescriptor::StructuredBufferSrv(0, BufferResourceView::create(m_scene_system.get_transform_info_buffer())),
        WriteDescriptor::StructuredBufferSrv(1, draw_data_view),
    };

    const auto descriptor_set = g_render_device->allocate_descriptor_set(
        DrawListsSystemLayout::get_layout(), DescriptorSetAllocationType::Transient);
    descriptor_set->update(writes);

    return descriptor_set;
}

//
// Functions
//

static DrawListSystem* s_draw_list_system = nullptr;

void draw_list_system_init(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool)
{
    MIZU_ASSERT(s_draw_list_system == nullptr, "DrawListSystem has already been initialized");
    s_draw_list_system = new DrawListSystem{scene_system, gpu_mesh_pool};
}

void draw_list_system_shutdown()
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");

    delete s_draw_list_system;
    s_draw_list_system = nullptr;
}

void draw_list_system_reset()
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->reset();
}

void draw_list_system_add_passes(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->add_passes(builder, frame_allocator);
}

void draw_list_system_finalize(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->finalize(builder, frame_allocator);
}

void draw_list_system_prepare(FrameLinearAllocator& frame_allocator)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->prepare(frame_allocator);
}

DrawListHandle create_draw_list(const DrawListRequest& request)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    return s_draw_list_system->create_draw_list(request);
}

void dispatch_draw_list(CommandBuffer& command, DrawListHandle handle, const DrawListRasterPassInfo& info)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->dispatch_draw_list(command, handle, info);
}

} // namespace Mizu
