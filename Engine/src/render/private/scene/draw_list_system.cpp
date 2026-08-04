#include "render/scene/draw_list_system.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <span>

#include "asset/asset_handle.h"
#include "asset/asset_registry.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "base/math/aabb.h"
#include "base/utils/hash.h"
#include "core/game_context.h"
#include "core/runtime.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render.pipeline/scene_renderer_shaders.h"
#include "render.pipeline/scene_shaders.h"
#include "render/runtime/renderer_settings.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"
#include "render/systems/pipeline_cache.h"
#include "resources/gpu_pools.h"
#include "scene/scene_system.h"

namespace Mizu
{

static constexpr size_t CACHE_LINE = std::hardware_destructive_interference_size;
static constexpr size_t DRAW_ELEMENTS_STRIDE =
    (StaticMeshConfig::MaxNumHandles + CACHE_LINE - 1) / CACHE_LINE * CACHE_LINE;

static constexpr uint64_t MAX_DRAW_INDIRECT_COMMANDS = 1000;

struct DrawElement
{
    GpuMeshDrawPayload mesh_draw{};

    ShaderInstance vertex_instance{};
    ShaderInstance fragment_instance{};

    uint32_t instance_count = 0;
    uint32_t material_buffer_offset = std::numeric_limits<uint32_t>::max();
    uint32_t transform_buffer_offset = std::numeric_limits<uint32_t>::max();
    uint32_t draw_index = std::numeric_limits<uint32_t>::max();

    size_t sort_key = 0;
    size_t pipeline_hash = 0;

#if MIZU_DEBUG
    std::string_view debug_name;
#endif
};

// Must match GpuDrawableInstance in GpuDrivenRendering.slang
struct GpuDrawableInstance
{
    glm::vec3 aabbMin;
    uint32_t transformSlot;
    glm::vec3 aabbMax;
    uint32_t materialOffset;

    uint32_t indexCount;
    uint32_t firstIndex;
    uint32_t firstVertex;
    uint32_t _pad{};
};

// Must match with GpuDrawData in GpuDrivenRendering.slang
struct GpuDrawData
{
    uint32_t transformSlot;
    uint32_t materialOffset;
};

// Must match GpuCullParams in CompileDrawLists.slang
struct GpuCullParams
{
    glm::vec4 planes[6];
    uint32_t frustumMask;

    uint32_t _pad[3]{};
};

// We need this here so that we can keep `DrawElement` and `GpuDrawData` in the cpp.
DrawListSystem::~DrawListSystem() = default;

DrawListSystem::DrawListSystem(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool)
    : m_scene_system(scene_system)
    , m_gpu_mesh_pool(gpu_mesh_pool)
{
    constexpr size_t DRAW_ELEMENTS_SIZE = MAX_NUM_COMPILE_LISTS * DRAW_ELEMENTS_STRIDE;

    m_draw_elements.resize(DRAW_ELEMENTS_SIZE);
    m_draw_data.resize(DRAW_ELEMENTS_SIZE);

    const RendererSettings& settings = get_setting<RendererSettings>();
    m_gpu_driven_rendering_enabled = settings.gpu_driven_rendering_enabled;
}

void DrawListSystem::reset()
{
    m_draw_list_cache.clear();
    m_compile_list_cache.clear();

    m_num_draw_lists.store(0, std::memory_order_relaxed);
    m_num_compile_lists.store(0, std::memory_order_relaxed);

    for (DrawListRecord& record : m_draw_list_records)
    {
        record = DrawListRecord{};
    }

    for (CompileListRecord& compile_list : m_compile_list_records)
    {
        compile_list = CompileListRecord{};
    }
}

void DrawListSystem::build_frame_resources(FrameLinearAllocator& linear_allocator)
{
    const uint32_t num_compile_lists = m_num_compile_lists.load(std::memory_order_relaxed);

    for (uint32_t i = 0; i < num_compile_lists; ++i)
    {
        CompileListRecord& compile_list = m_compile_list_records[i];

        if (!m_gpu_driven_rendering_enabled && !compile_list.is_compiled)
        {
            MIZU_LOG_ERROR("Compile list at index {} has not been compiled yet, skipping.", i);
            continue;
        }

        if (!m_gpu_driven_rendering_enabled && compile_list.num_draw_elements == 0)
        {
            continue;
        }

        const std::span<const GpuDrawData> draw_data_span =
            std::span(m_draw_data.data() + compile_list.draw_elements_offset, compile_list.num_draw_data);

        const FrameAllocation draw_data_allocation =
            linear_allocator.allocate_structured<GpuDrawData>(compile_list.num_draw_data);
        draw_data_allocation.upload(draw_data_span);

        compile_list.draw_data_allocation = draw_data_allocation;
    }
}

static size_t hash_frustum_mask(const FrustumMask& mask)
{
    return hash_compute(mask.top, mask.bottom, mask.left, mask.right, mask.near, mask.far);
}

static size_t hash_frustum(const Frustum& frustum)
{
    size_t h = 0;

    hash_combine(h, frustum.center.x, frustum.center.y, frustum.center.z);
    hash_combine(h, frustum.near.distance, frustum.far.distance);

    return h;
}

static size_t hash_compiled_draw_list(const DrawListRequest& request)
{
    size_t h = 0;

    hash_combine(h, hash_frustum_mask(request.frustum_mask));
    hash_combine(h, request.frustum.has_value());

    if (request.frustum.has_value())
    {
        hash_combine(h, hash_frustum(*request.frustum));
    }

    return h;
}

static size_t hash_draw_list(const DrawListRequest& request)
{
    size_t h = 0;

    hash_combine(h, request.raster_pass);
    hash_combine(h, hash_compiled_draw_list(request));

    return h;
}

DrawListHandle DrawListSystem::create_draw_list(const DrawListRequest& request)
{
    MIZU_ASSERT(request.raster_pass != nullptr, "Can't create draw list without a DrawListRasterPass");
    MIZU_ASSERT(request.view_count > 0, "View count must be greater than 0");

    if (m_gpu_driven_rendering_enabled)
    {
        // Register buffer resources for lifetime purposes

        const TransientGpuDrivenRenderingResources& resources = m_transient_gpu_driven_rendering_resources;

        if (resources.indirect_command_buffer.is_valid())
        {
            request.pass_builder.indirect_argument(resources.indirect_command_buffer);
            request.pass_builder.indirect_argument(resources.indirect_count_buffer);
            request.pass_builder.read(resources.draw_data_buffer);
        }
    }

    const size_t draw_list_hash = hash_draw_list(request);

    const auto cache_it = m_draw_list_cache.find(draw_list_hash);
    if (cache_it != m_draw_list_cache.end())
    {
        return cache_it->second;
    }

    const size_t compiled_hash = hash_compiled_draw_list(request);

    uint32_t compile_idx;
    const auto compiled_cache_it = m_compile_list_cache.find(compiled_hash);
    if (compiled_cache_it != m_compile_list_cache.end())
    {
        compile_idx = compiled_cache_it->second;
    }
    else
    {
        compile_idx = m_num_compile_lists.fetch_add(1, std::memory_order_relaxed);
        if (compile_idx >= MAX_NUM_COMPILE_LISTS)
        {
            m_num_compile_lists.fetch_sub(1, std::memory_order_relaxed);
            MIZU_ASSERT(false, "Exceeded maximum number of compile lists ({})", MAX_NUM_COMPILE_LISTS);
            return DrawListHandle{};
        }

        CompileListRecord& compile_list = m_compile_list_records[compile_idx];
        compile_list.frustum = request.frustum;
        compile_list.frustum_mask = request.frustum_mask;

        m_compile_list_cache.insert({compiled_hash, compile_idx});
    }

    const uint32_t draw_list_index = m_num_draw_lists.fetch_add(1, std::memory_order_relaxed);

    if (draw_list_index >= MAX_NUM_DRAW_LISTS)
    {
        m_num_draw_lists.fetch_sub(1, std::memory_order_relaxed);

        MIZU_ASSERT(false, "Exceeded maximum number of draw lists ({})", MAX_NUM_DRAW_LISTS);
        return DrawListHandle{};
    }

    m_draw_list_records[draw_list_index] = DrawListRecord{
        .raster_pass = request.raster_pass,
        .view_count = request.view_count,
        .compiled_draw_list_idx = compile_idx,
    };

    const DrawListHandle handle{.index = draw_list_index};
    m_draw_list_cache.insert({draw_list_hash, handle});

    return handle;
}

void DrawListSystem::compile_draw_lists()
{
    if (m_gpu_driven_rendering_enabled)
        return;

    const uint32_t num_compile_lists = m_num_compile_lists.load(std::memory_order_relaxed);
    if (num_compile_lists == 0)
        return;

    PendingBatch compile_batch = g_job_system->schedule_batch();

    for (uint32_t i = 0; i < num_compile_lists; ++i)
    {
        compile_batch.add(&DrawListSystem::compile_draw_list_job, this, i);
    }

    const JobHandle compile_job_handle = compile_batch.submit();
    g_job_system->wait_for(compile_job_handle);
}

void DrawListSystem::add_compile_draw_lists_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    if (!m_gpu_driven_rendering_enabled)
        return;

    const std::span<const SceneDrawableInfo> drawables = m_scene_system.get_drawables();
    if (drawables.empty())
        return;

    std::vector<GpuDrawableInstance> gpu_drawable_instances(drawables.size());
    for (size_t i = 0; i < drawables.size(); ++i)
    {
        const SceneDrawableInfo& drawable = drawables[i];
        gpu_drawable_instances[i] = GpuDrawableInstance{
            .aabbMin = drawable.gpu_mesh_record.payload.bounding_box.min(),
            .transformSlot = drawable.transform_slot_index,
            .aabbMax = drawable.gpu_mesh_record.payload.bounding_box.max(),
            .materialOffset = drawable.material_buffer_offset,
            .indexCount = drawable.gpu_mesh_draw.index_count,
            .firstIndex = drawable.gpu_mesh_draw.first_index,
            .firstVertex = drawable.gpu_mesh_draw.first_vertex,
        };
    }

    const FrameAllocation gpu_drawables_allocation =
        frame_allocator.allocate_structured<GpuDrawableInstance>(gpu_drawable_instances.size());
    gpu_drawables_allocation.upload(gpu_drawable_instances);

    BufferDescription indirect_command_buffer_desc{};
    indirect_command_buffer_desc.size =
        sizeof(DrawIndexedIndirectCommand) * MAX_DRAW_INDIRECT_COMMANDS * MAX_NUM_DRAW_LISTS;
    indirect_command_buffer_desc.stride = sizeof(DrawIndexedIndirectCommand);
    indirect_command_buffer_desc.usage =
        BufferUsageBits::UnorderedAccess | BufferUsageBits::TransferDst | BufferUsageBits::IndirectBuffer;
    indirect_command_buffer_desc.name = "DrawListSystem::IndirectCommandBuffer";
    const RenderGraphResource indirect_command_buffer = builder.create_buffer(indirect_command_buffer_desc);

    BufferDescription indirect_count_buffer_desc{};
    indirect_count_buffer_desc.size = sizeof(uint32_t) * MAX_DRAW_INDIRECT_COMMANDS;
    indirect_count_buffer_desc.stride = sizeof(uint32_t);
    indirect_count_buffer_desc.usage = BufferUsageBits::UnorderedAccess | BufferUsageBits::TransferDst
                                       | BufferUsageBits::IndirectBuffer | BufferUsageBits::ShaderResource;
    indirect_count_buffer_desc.name = "DrawListSystem::IndirectCountBuffer";
    const RenderGraphResource indirect_count_buffer = builder.create_buffer(indirect_count_buffer_desc);

    BufferDescription gpu_draw_data_buffer_desc{};
    gpu_draw_data_buffer_desc.size = sizeof(GpuDrawData) * MAX_DRAW_INDIRECT_COMMANDS * MAX_NUM_DRAW_LISTS;
    gpu_draw_data_buffer_desc.stride = sizeof(GpuDrawData);
    gpu_draw_data_buffer_desc.usage = BufferUsageBits::ShaderResource | BufferUsageBits::UnorderedAccess;
    gpu_draw_data_buffer_desc.name = "DrawListSystem::GpuDrawDataBuffer";
    const RenderGraphResource gpu_draw_data_buffer = builder.create_buffer(gpu_draw_data_buffer_desc);

    const RenderGraphResource visible_indices_buffer = builder.create_structured_buffer<uint32_t>(
        drawables.size() * MAX_NUM_COMPILE_LISTS, "DrawListSystem::VisibleIndicesBuffer");

    m_transient_gpu_driven_rendering_resources = TransientGpuDrivenRenderingResources{
        .indirect_command_buffer = indirect_command_buffer,
        .indirect_count_buffer = indirect_count_buffer,
        .draw_data_buffer = gpu_draw_data_buffer,
    };

    struct ClearBuffersPassData
    {
        RenderGraphResource indirect_count_buffer;
        RenderGraphResource indirect_command_buffer;
    };

    builder.add_pass<ClearBuffersPassData>(
        "DrawListSystem::ClearIndirectBuffers",
        [&](RenderGraphPassBuilder& pass, ClearBuffersPassData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.indirect_count_buffer = pass.write(indirect_count_buffer);
            data.indirect_command_buffer = pass.write(indirect_command_buffer);
        },
        [](CommandBuffer& command, const ClearBuffersPassData& data, const RenderGraphPassResources& resources) {
            const auto indirect_count_buffer = resources.get_buffer(data.indirect_count_buffer);
            const auto indirect_command_buffer = resources.get_buffer(data.indirect_command_buffer);

            command.fill_buffer(*indirect_count_buffer, 0);
            command.fill_buffer(*indirect_command_buffer, 0);
        });

    struct CullingPassData
    {
        RenderGraphResource indirect_count_buffer;
        RenderGraphResource visible_indices_buffer;

        FrameAllocation gpu_drawables_allocation;
        uint32_t num_drawables;
    };

    builder.add_pass<CullingPassData>(
        "DrawListSystem::CullInstances",
        [&](RenderGraphPassBuilder& pass, CullingPassData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.indirect_count_buffer = pass.write(indirect_count_buffer);
            data.visible_indices_buffer = pass.write(visible_indices_buffer);

            data.gpu_drawables_allocation = gpu_drawables_allocation;
            data.num_drawables = static_cast<uint32_t>(drawables.size());
        },
        [this, &frame_allocator](
            CommandBuffer& command, const CullingPassData& data, const RenderGraphPassResources& resources) {
            const uint32_t num_compile_lists = m_num_compile_lists.load(std::memory_order_relaxed);
            if (num_compile_lists == 0)
                return;

            std::vector<GpuCullParams> gpu_cull_params(num_compile_lists);
            for (size_t i = 0; i < num_compile_lists; ++i)
            {
                const CompileListRecord& record = m_compile_list_records[i];

                GpuCullParams cull_params{};

                if (record.frustum.has_value())
                {
                    cull_params.frustumMask = record.frustum_mask.to_uint8();

                    cull_params.planes[0] = record.frustum->top.to_vec4();
                    cull_params.planes[1] = record.frustum->bottom.to_vec4();
                    cull_params.planes[2] = record.frustum->left.to_vec4();
                    cull_params.planes[3] = record.frustum->right.to_vec4();
                    cull_params.planes[4] = record.frustum->near.to_vec4();
                    cull_params.planes[5] = record.frustum->far.to_vec4();
                }
                else
                {
                    cull_params.frustumMask = 0;
                }

                gpu_cull_params[i] = cull_params;
            }

            const FrameAllocation gpu_cull_params_allocation =
                frame_allocator.allocate_structured<GpuCullParams>(gpu_cull_params.size());
            gpu_cull_params_allocation.upload(gpu_cull_params);

            const auto indirect_count_buffer = resources.get_buffer(data.indirect_count_buffer);
            const auto visible_indices_buffer = resources.get_buffer(data.visible_indices_buffer);

            const auto pipeline = get_compute_pipeline(DrawListCullInstancesCS{});

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(Layout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute) // g_instances
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Compute) // g_transformInfo
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::Compute) // g_cullParams
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(0, 1, ShaderType::Compute) // g_visibleIndices
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(1, 1, ShaderType::Compute) // g_visibleCount
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, data.gpu_drawables_allocation.view),
                WriteDescriptor::StructuredBufferSrv(
                    1, BufferResourceView::create(m_scene_system.get_transform_info_buffer())),
                WriteDescriptor::StructuredBufferSrv(2, gpu_cull_params_allocation.view),
                WriteDescriptor::StructuredBufferUav(0, BufferResourceView::create(visible_indices_buffer)),
                WriteDescriptor::StructuredBufferUav(
                    1, BufferResourceView::create(resources.get_buffer(data.indirect_count_buffer))),
            };

            const auto descriptor_set =
                g_render_device->allocate_descriptor_set(Layout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.bind_pipeline(pipeline);
            command.bind_descriptor_set(descriptor_set, 0);

            const glm::uvec3 culling_group_count = compute_group_count(
                glm::uvec3{data.num_drawables, 1, 1}, glm::uvec3{DrawListCullInstancesCS::GROUP_SIZE, 1, 1});

            struct CullingPushConstant
            {
                uint32_t compileListIdx;
                uint32_t outputOffset;
            } culling_push_constant{};

            for (uint32_t i = 0; i < num_compile_lists; ++i)
            {
                CompileListRecord& record = m_compile_list_records[i];
                record.is_compiled = true;

                culling_push_constant = CullingPushConstant{
                    .compileListIdx = i,
                    .outputOffset = i * data.num_drawables,
                };

                command.push_constant(culling_push_constant);

                command.dispatch(culling_group_count);
            }
        });

    struct CompileCommandsData
    {
        RenderGraphResource indirect_command_buffer;
        RenderGraphResource indirect_count_buffer;
        RenderGraphResource visible_indices_buffer;
        RenderGraphResource gpu_draw_data_buffer;

        FrameAllocation gpu_drawables_allocation;
        uint32_t num_drawables;
    };

    builder.add_pass<CompileCommandsData>(
        "DrawListSystem::CompileCommands",
        [&](RenderGraphPassBuilder& pass, CompileCommandsData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.indirect_command_buffer = pass.write(indirect_command_buffer);
            data.indirect_count_buffer = pass.read(indirect_count_buffer);
            data.visible_indices_buffer = pass.read(visible_indices_buffer);
            data.gpu_draw_data_buffer = pass.write(gpu_draw_data_buffer);

            data.gpu_drawables_allocation = gpu_drawables_allocation;
            data.num_drawables = static_cast<uint32_t>(drawables.size());
        },
        [=, this](CommandBuffer& command, const CompileCommandsData& data, const RenderGraphPassResources& resources) {
            const uint32_t num_compile_lists = m_num_compile_lists.load(std::memory_order_relaxed);
            if (num_compile_lists == 0)
                return;

            const uint32_t num_draw_lists = m_num_draw_lists.load(std::memory_order_relaxed);

            const auto indirect_command_buffer = resources.get_buffer(data.indirect_command_buffer);
            const auto indirect_count_buffer = resources.get_buffer(data.indirect_count_buffer);
            const auto visible_indices_buffer = resources.get_buffer(data.visible_indices_buffer);
            const auto gpu_draw_data_buffer = resources.get_buffer(data.gpu_draw_data_buffer);

            TransientGpuDrivenRenderingResources& gpu_driven_resources = m_transient_gpu_driven_rendering_resources;
            gpu_driven_resources.gpu_indirect_command_buffer = indirect_command_buffer.get();
            gpu_driven_resources.gpu_indirect_count_buffer = indirect_count_buffer.get();
            gpu_driven_resources.gpu_draw_data_buffer = gpu_draw_data_buffer.get();

            const auto pipeline = get_compute_pipeline(DrawListGenerateCommandsCS{});

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(Layout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute) // g_instances
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(3, 1, ShaderType::Compute) // g_visibleIndices
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(4, 1, ShaderType::Compute) // g_visibleCount
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(2, 1, ShaderType::Compute) // g_indirectCommands
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(3, 1, ShaderType::Compute) // g_gpuDrawData
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, data.gpu_drawables_allocation.view),
                WriteDescriptor::StructuredBufferSrv(3, BufferResourceView::create(visible_indices_buffer)),
                WriteDescriptor::StructuredBufferSrv(
                    4, BufferResourceView::create(resources.get_buffer(data.indirect_count_buffer))),
                WriteDescriptor::StructuredBufferUav(
                    2, BufferResourceView::create(resources.get_buffer(data.indirect_command_buffer))),
                WriteDescriptor::StructuredBufferUav(3, BufferResourceView::create(gpu_draw_data_buffer)),
            };

            const auto descriptor_set =
                g_render_device->allocate_descriptor_set(Layout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.bind_pipeline(pipeline);
            command.bind_descriptor_set(descriptor_set, 0);

            const glm::uvec3 generation_group_count = compute_group_count(
                glm::uvec3{data.num_drawables, 1, 1}, glm::uvec3{DrawListGenerateCommandsCS::GROUP_SIZE, 1, 1});

            struct GenerationPushConstant
            {
                uint32_t indirectCommandsOffset;
                uint32_t visibleIndicesOffset;
                uint32_t compileListIdx;
                uint32_t viewCount;
            } generation_push_constant{};

            for (uint32_t i = 0; i < num_draw_lists; ++i)
            {
                DrawListRecord& record = m_draw_list_records[i];
                MIZU_ASSERT(
                    record.compiled_draw_list_idx != std::numeric_limits<uint32_t>::max(),
                    "Draw list at index {} has invalid compile list index",
                    i);

                generation_push_constant = GenerationPushConstant{
                    .indirectCommandsOffset = i * static_cast<uint32_t>(MAX_DRAW_INDIRECT_COMMANDS),
                    .visibleIndicesOffset = record.compiled_draw_list_idx * data.num_drawables,
                    .compileListIdx = record.compiled_draw_list_idx,
                    .viewCount = record.view_count,
                };

                record.gpu_driven_indirect_commands_element_offset = generation_push_constant.indirectCommandsOffset;
                record.gpu_driven_indirect_count_element_offset = generation_push_constant.compileListIdx;

                command.push_constant(generation_push_constant);
                command.dispatch(generation_group_count);
            }

            // TODO: Manually transition to BufferResourceState::IndirectArgument because, most likely,
            // the users of `dispatch_draw_list` will not register the indirect command and count
            // buffers into the RenderGraph for automatic transitions.
            command.transition_resource(
                *indirect_command_buffer, BufferResourceState::UnorderedAccess, BufferResourceState::IndirectArgument);
            command.transition_resource(
                *indirect_count_buffer, BufferResourceState::ShaderReadOnly, BufferResourceState::IndirectArgument);
            command.transition_resource(
                *gpu_draw_data_buffer, BufferResourceState::UnorderedAccess, BufferResourceState::ShaderReadOnly);
        });
}

void DrawListSystem::dispatch_draw_list(
    CommandBuffer& command,
    DrawListHandle handle,
    const DrawListRasterPassInfo& info)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(handle.is_valid(), "Invalid draw list handle");
    MIZU_ASSERT(
        handle.index < m_num_draw_lists.load(std::memory_order_relaxed), "Draw list handle index is out of range");

    if (m_gpu_driven_rendering_enabled)
    {
        dispatch_draw_list_gpu(command, handle, info);
    }
    else
    {
        dispatch_draw_list_cpu(command, handle, info);
    }
}

static size_t create_sort_key(size_t pipeline_hash, MeshAssetHandle mesh_handle, MaterialAssetHandle material_handle)
{
    return hash_compute(pipeline_hash, mesh_handle.get_id(), material_handle.get_id());
}

static size_t create_pipeline_hash(const ShaderInstance& vertex_instance, const ShaderInstance& fragment_instance)
{
    const auto shader_hash = [](const ShaderInstance& instance) -> size_t {
        return hash_compute(
            instance.virtual_path, instance.entry_point, instance.type, instance.environment.get_hash());
    };

    return hash_compute(shader_hash(vertex_instance), shader_hash(fragment_instance));
}

void DrawListSystem::compile_draw_list_job(uint32_t compile_list_idx)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(
        compile_list_idx < m_num_compile_lists.load(std::memory_order_relaxed), "Compile list index is out of range");

    CompileListRecord& compile_list = m_compile_list_records[compile_list_idx];
    const std::optional<Frustum>& frustum = compile_list.frustum;
    const FrustumMask& frustum_mask = compile_list.frustum_mask;

    const std::span<const SceneDrawableInfo> drawables = m_scene_system.get_drawables();

    uint32_t num_draw_elements = 0;
    const uint32_t draw_elements_offset = compile_list_idx * DRAW_ELEMENTS_STRIDE;

    // TODO: Hardcoding the shaders here until we have material instances as assets
    PbrOpaqueMaterialShaderVS vertex_shader{};
    PbrOpaqueMaterialShaderFS fragment_shader{};

    for (const SceneDrawableInfo& drawable : drawables)
    {
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

        if (frustum.has_value())
        {
            const AABB& local_aabb = drawable.gpu_mesh_record.payload.bounding_box;

            const TransformDynamicState& ts =
                g_transform_state_manager->rend_get_dynamic_state(drawable.transform_handle);

            glm::mat4 world_transform{1.0f};
            world_transform = glm::translate(world_transform, ts.translation);
            world_transform = glm::rotate(world_transform, ts.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            world_transform = glm::rotate(world_transform, ts.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            world_transform = glm::rotate(world_transform, ts.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            world_transform = glm::scale(world_transform, ts.scale);

            const AABB world_aabb = transform_aabb(local_aabb, world_transform);

            if (!frustum->is_inside_frustum(world_aabb, frustum_mask))
                continue;
        }

        const size_t pipeline_hash = create_pipeline_hash(vertex_shader.get_instance(), fragment_shader.get_instance());
        const size_t sort_key = create_sort_key(pipeline_hash, drawable.mesh_handle, drawable.material_handle);

#if MIZU_DEBUG
        const AssetRegistry& asset_registry = g_game_context->get_asset_registry();

        std::string_view debug_name = asset_registry.get_virtual_path(drawable.mesh_handle);
        if (debug_name.empty())
            debug_name = "Mesh";
#endif

        m_draw_elements[draw_elements_offset + num_draw_elements] = DrawElement{
            .mesh_draw = drawable.gpu_mesh_draw,
            .vertex_instance = vertex_shader.get_instance(),
            .fragment_instance = fragment_shader.get_instance(),
            .instance_count = 1,
            .material_buffer_offset = drawable.material_buffer_offset,
            .transform_buffer_offset = drawable.transform_slot_index,
            .draw_index = 0,
            .sort_key = sort_key,
            .pipeline_hash = pipeline_hash,
#if MIZU_DEBUG
            .debug_name = debug_name,
#endif
        };

        num_draw_elements += 1;
    }

    if (num_draw_elements == 0)
    {
        compile_list = CompileListRecord{
            .is_compiled = true,
            .frustum = frustum,
            .frustum_mask = frustum_mask,
            .num_draw_elements = 0,
            .num_draw_data = 0,
            .draw_elements_offset = draw_elements_offset,
        };

        return;
    }

    auto begin = m_draw_elements.begin() + draw_elements_offset;
    auto end = begin + num_draw_elements;

    std::sort(begin, end, [](const DrawElement& a, const DrawElement& b) {
        if (a.pipeline_hash != b.pipeline_hash)
            return a.pipeline_hash < b.pipeline_hash;

        if (a.material_buffer_offset != b.material_buffer_offset)
            return a.material_buffer_offset < b.material_buffer_offset;

        return a.sort_key < b.sort_key;
    });

    size_t current_sort_key = begin[0].sort_key;
    uint32_t current_instance_offset = 0;
    uint32_t move_backwards_offset = 0;

    DrawElement& first = begin[0];

    m_draw_data[draw_elements_offset] = GpuDrawData{
        .transformSlot = first.transform_buffer_offset,
        .materialOffset = first.material_buffer_offset,
    };
    first.draw_index = 0;

    const uint32_t num_draw_data = num_draw_elements;

    for (uint32_t i = 1; i < num_draw_data; ++i)
    {
        DrawElement& element = begin[i];

        // Elements merged into the same run share a material, `sort_key` includes the material handle, so writing the
        // element's own offset for every entry of the run is correct.
        m_draw_data[draw_elements_offset + i] = GpuDrawData{
            .transformSlot = element.transform_buffer_offset,
            .materialOffset = element.material_buffer_offset,
        };

        if (element.sort_key == current_sort_key)
        {
            begin[current_instance_offset].instance_count += 1;
            move_backwards_offset += 1;

            num_draw_elements -= 1;
        }
        else
        {
            current_sort_key = element.sort_key;
            current_instance_offset = i - move_backwards_offset;

            begin[i - move_backwards_offset] = element;
            begin[i - move_backwards_offset].draw_index = i;
        }
    }

    compile_list.is_compiled = true;
    compile_list.num_draw_elements = num_draw_elements;
    compile_list.num_draw_data = num_draw_data;
    compile_list.draw_elements_offset = draw_elements_offset;
}

void DrawListSystem::dispatch_draw_list_cpu(
    CommandBuffer& command,
    DrawListHandle handle,
    const DrawListRasterPassInfo& info)
{
    const DrawListRecord& record = m_draw_list_records[handle.index];
    const CompileListRecord& compile_list = m_compile_list_records[record.compiled_draw_list_idx];

    if (!compile_list.is_compiled)
    {
        MIZU_LOG_ERROR("Draw list at index {} has not been compiled yet, skipping.", handle.index);
        return;
    }

    if (compile_list.num_draw_elements == 0)
    {
        return;
    }

    const BufferResource& vertex_buffer = *m_gpu_mesh_pool.get_vertex_buffer();
    const BufferResource& index_buffer = *m_gpu_mesh_pool.get_index_buffer();

    command.bind_vertex_buffer(vertex_buffer);
    command.bind_index_buffer(index_buffer);

    const auto draw_elements_begin = m_draw_elements.begin() + compile_list.draw_elements_offset;

    bool pipeline_bound = false;
    size_t last_pipeline_hash = 0;

    const DrawListRasterPass* raster_pass = record.raster_pass;
    const uint32_t view_count = record.view_count;

    for (size_t i = 0; i < compile_list.num_draw_elements; ++i)
    {
        const DrawElement& element = draw_elements_begin[static_cast<ptrdiff_t>(i)];

        const DrawItem draw_item{
            .vertex_instance = element.vertex_instance,
            .fragment_instance = element.fragment_instance,
            .pipeline_hash = element.pipeline_hash,
        };

        const size_t pipeline_hash = raster_pass->get_pipeline_hash(draw_item);
        if (!pipeline_bound || pipeline_hash != last_pipeline_hash)
        {
            const ShaderInstance vertex_shader = raster_pass->get_vertex_shader(draw_item);
            const ShaderInstance fragment_shader = raster_pass->get_fragment_shader(draw_item);

            const auto pipeline = get_graphics_pipeline(
                vertex_shader,
                fragment_shader,
                info.rasterization_state,
                info.depth_stencil_state,
                info.color_blend_state,
                info.framebuffer_info);

            command.bind_pipeline(pipeline);

            const DrawListRasterBindings& bindings = info.bindings;

            // TODO: By default setting the DrawListSystem resources at set 0, this could be problematic as it's not
            // clear to the user that we're doing this.
            bind_resources(command, handle, 0);

            for (uint32_t set = 0; set < MAX_DESCRIPTOR_SET_COUNT; ++set)
            {
                const std::shared_ptr<DescriptorSet>& descriptor_set = bindings.descriptor_sets[set];
                if (descriptor_set != nullptr)
                {
                    command.bind_descriptor_set(descriptor_set, set);
                }
            }

            last_pipeline_hash = pipeline_hash;
            pipeline_bound = true;
        }

        bind_draw_index_push_constant(command, element.draw_index);

#if MIZU_DEBUG
        command.begin_gpu_marker(element.debug_name);
#endif

        const uint32_t instance_count = element.instance_count * view_count;
        command.draw_indexed(
            element.mesh_draw.index_count,
            element.mesh_draw.first_index,
            element.mesh_draw.first_vertex,
            instance_count,
            0);

#if MIZU_DEBUG
        command.end_gpu_marker();
#endif
    }
}

void DrawListSystem::dispatch_draw_list_gpu(
    CommandBuffer& command,
    DrawListHandle handle,
    const DrawListRasterPassInfo& info)
{
    const DrawListRecord& record = m_draw_list_records[handle.index];
    const CompileListRecord& compile_list = m_compile_list_records[record.compiled_draw_list_idx];

    if (!compile_list.is_compiled)
        return;

    const BufferResource& vertex_buffer = *m_gpu_mesh_pool.get_vertex_buffer();
    const BufferResource& index_buffer = *m_gpu_mesh_pool.get_index_buffer();

    command.bind_vertex_buffer(vertex_buffer);
    command.bind_index_buffer(index_buffer);

    // TODO: For the moment supposing a single pipeline, like we do in `compile_draw_list_job`.
    PbrOpaqueMaterialShaderVS vertex_shader{};
    PbrOpaqueMaterialShaderFS fragment_shader{};

    const DrawItem draw_item{
        .vertex_instance = vertex_shader.get_instance(),
        .fragment_instance = fragment_shader.get_instance(),
        .pipeline_hash = create_pipeline_hash(vertex_shader.get_instance(), fragment_shader.get_instance()),
    };

    const DrawListRasterPass* raster_pass = record.raster_pass;

    const auto pipeline = get_graphics_pipeline(
        raster_pass->get_vertex_shader(draw_item),
        raster_pass->get_fragment_shader(draw_item),
        info.rasterization_state,
        info.depth_stencil_state,
        info.color_blend_state,
        info.framebuffer_info);

    command.bind_pipeline(pipeline);

    bind_resources(command, handle, 0);

    for (uint32_t set = 0; set < MAX_DESCRIPTOR_SET_COUNT; ++set)
    {
        const std::shared_ptr<DescriptorSet>& descriptor_set = info.bindings.descriptor_sets[set];
        if (descriptor_set != nullptr)
        {
            command.bind_descriptor_set(descriptor_set, set);
        }
    }

    bind_draw_index_push_constant(command, record.gpu_driven_indirect_commands_element_offset);

    const BufferResource* indirect_command_buffer =
        m_transient_gpu_driven_rendering_resources.gpu_indirect_command_buffer;
    const BufferResource* indirect_count_buffer = m_transient_gpu_driven_rendering_resources.gpu_indirect_count_buffer;

    command.draw_indexed_indirect_count(
        *indirect_command_buffer,
        record.gpu_driven_indirect_commands_element_offset * sizeof(DrawIndexedIndirectCommand),
        *indirect_count_buffer,
        record.gpu_driven_indirect_count_element_offset * sizeof(uint32_t),
        static_cast<uint32_t>(MAX_DRAW_INDIRECT_COMMANDS),
        sizeof(DrawIndexedIndirectCommand));
}

void DrawListSystem::bind_resources(CommandBuffer& command, DrawListHandle handle, uint32_t set) const
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");
    MIZU_ASSERT(
        handle.index < m_num_draw_lists.load(std::memory_order_relaxed), "Draw list handle index is out of range");

    const DrawListRecord& record = m_draw_list_records[handle.index];
    const CompileListRecord& compile_list = m_compile_list_records[record.compiled_draw_list_idx];

    BufferResourceView draw_data_view{};
    if (m_gpu_driven_rendering_enabled)
    {
        BufferResource* gpu_draw_data_buffer = m_transient_gpu_driven_rendering_resources.gpu_draw_data_buffer;

        MIZU_ASSERT(gpu_draw_data_buffer != nullptr, "Gpu draw data buffer has not been resolved");
        draw_data_view = BufferResourceView::create(gpu_draw_data_buffer);
    }
    else
    {
        if (compile_list.num_draw_elements == 0)
            return;

        draw_data_view = compile_list.draw_data_allocation.view;
    }

    // clang-format off
    MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(DrawListsSystemLayout)
        MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex) // g_transformInfo
        MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Vertex) // g_drawData
    MIZU_END_DESCRIPTOR_SET_LAYOUT()
    // clang-format on

    const std::array writes = {
        WriteDescriptor::StructuredBufferSrv(0, BufferResourceView::create(m_scene_system.get_transform_info_buffer())),
        WriteDescriptor::StructuredBufferSrv(1, draw_data_view),
    };

    const auto descriptor_set = g_render_device->allocate_descriptor_set(
        DrawListsSystemLayout::get_layout(), DescriptorSetAllocationType::Transient);
    descriptor_set->update(writes);

    command.bind_descriptor_set(descriptor_set, set);
}

void DrawListSystem::bind_draw_index_push_constant(CommandBuffer& command, uint32_t draw_index) const
{
    struct PushConstant
    {
        uint32_t draw_index;
    };

    command.push_constant<PushConstant>({
        .draw_index = draw_index,
    });
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

void draw_list_system_compile_draw_lists()
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->compile_draw_lists();
}

void draw_list_system_add_compile_draw_lists_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->add_compile_draw_lists_pass(builder, frame_allocator);
}

void draw_list_system_build_frame_resources(FrameLinearAllocator& linear_allocator)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->build_frame_resources(linear_allocator);
}

void draw_list_system_reset()
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    return s_draw_list_system->reset();
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
