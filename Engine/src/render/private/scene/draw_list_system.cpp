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

#include "render.pipeline/material_shaders.h"
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

struct DrawElement
{
    GpuMeshDrawPayload mesh_draw{};

    ShaderInstance vertex_instance{};
    ShaderInstance fragment_instance{};

    uint32_t instance_count = 0;
    uint32_t material_buffer_offset = std::numeric_limits<uint32_t>::max();
    uint32_t transform_buffer_offset = std::numeric_limits<uint32_t>::max();
    uint32_t view_indices_offset = std::numeric_limits<uint32_t>::max();

    size_t sort_key = 0;
    size_t pipeline_hash = 0;

#if MIZU_DEBUG
    std::string_view debug_name;
#endif
};

// We need this here so that we can keep `DrawElement` in the cpp file
DrawListSystem::~DrawListSystem() = default;

DrawListSystem::DrawListSystem(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool)
    : m_scene_system(scene_system)
    , m_gpu_mesh_pool(gpu_mesh_pool)
{
    constexpr size_t DRAW_ELEMENTS_SIZE = MAX_NUM_COMPILE_LISTS * DRAW_ELEMENTS_STRIDE;

    m_draw_elements.resize(DRAW_ELEMENTS_SIZE);
    m_view_indices.resize(DRAW_ELEMENTS_SIZE);
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

        if (!compile_list.is_compiled)
        {
            MIZU_LOG_ERROR("Compile list at index {} has not been compiled yet, skipping.", i);
            continue;
        }

        if (compile_list.num_draw_elements == 0)
        {
            continue;
        }

        const std::span<const uint32_t> view_indices_span =
            std::span(m_view_indices.data() + compile_list.draw_elements_offset, compile_list.num_view_indices);

        FrameAllocation view_indices_allocation =
            linear_allocator.allocate_structured<uint32_t>(compile_list.num_view_indices);
        view_indices_allocation.upload(view_indices_span);

        compile_list.view_indices_allocation = view_indices_allocation;
    }
}

void DrawListSystem::bind_resources(CommandBuffer& command, DrawListHandle handle, uint32_t set)
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");
    MIZU_ASSERT(
        handle.index < m_num_draw_lists.load(std::memory_order_relaxed), "Draw list handle index is out of range");

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

    // clang-format off
    MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(DrawListsSystemLayout)
        MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex) // transform_info
        MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Vertex) // view_indices
    MIZU_END_DESCRIPTOR_SET_LAYOUT()
    // clang-format on

    std::array writes = {
        WriteDescriptor::StructuredBufferSrv(0, BufferResourceView::create(m_scene_system.get_transform_info_buffer())),
        WriteDescriptor::StructuredBufferSrv(1, compile_list.view_indices_allocation.view),
    };

    const auto descriptor_set = g_render_device->allocate_descriptor_set(
        DrawListsSystemLayout::get_layout(), DescriptorSetAllocationType::Transient);
    descriptor_set->update(writes);

    command.bind_descriptor_set(descriptor_set, set);
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
        .compiled_draw_list_idx = compile_idx,
    };

    const DrawListHandle handle{.index = draw_list_index};
    m_draw_list_cache.insert({draw_list_hash, handle});

    return handle;
}

void DrawListSystem::compile_draw_lists()
{
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

void DrawListSystem::dispatch_draw_list(
    CommandBuffer& command,
    DrawListHandle handle,
    const DrawListRasterPassInfo& info,
    uint32_t view_count)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(handle.is_valid(), "Invalid draw list handle");
    MIZU_ASSERT(
        handle.index < m_num_draw_lists.load(std::memory_order_relaxed), "Draw list handle index is out of range");

    MIZU_ASSERT(view_count > 0, "View count must be greater than 0");

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

    const bool is_material_raster_pass = raster_pass->get_is_material_raster_pass();
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

        if (is_material_raster_pass)
        {
            bind_material_push_constant(command, element);
        }
        else
        {
            bind_default_push_constant(command, element);
        }

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
    PBROpaqueShaderVS vertex_shader{};
    PBROpaqueShaderFS fragment_shader{};

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
            debug_name = "Mesh without name";
#endif

        m_draw_elements[draw_elements_offset + num_draw_elements] = DrawElement{
            .mesh_draw = drawable.gpu_mesh_draw,
            .vertex_instance = vertex_shader.get_instance(),
            .fragment_instance = fragment_shader.get_instance(),
            .instance_count = 1,
            .material_buffer_offset = drawable.material_buffer_offset,
            .transform_buffer_offset = drawable.transform_slot_index,
            .view_indices_offset = 0,
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
            .num_view_indices = 0,
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

    m_view_indices[draw_elements_offset] = first.transform_buffer_offset;
    first.view_indices_offset = 0;

    const uint32_t num_view_indices = num_draw_elements;

    for (uint32_t i = 1; i < num_view_indices; ++i)
    {
        DrawElement& element = begin[i];

        m_view_indices[draw_elements_offset + i] = element.transform_buffer_offset;

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
            begin[i - move_backwards_offset].view_indices_offset = i;
        }
    }

    compile_list.is_compiled = true;
    compile_list.num_draw_elements = num_draw_elements;
    compile_list.num_view_indices = num_view_indices;
    compile_list.draw_elements_offset = draw_elements_offset;
}

void DrawListSystem::bind_default_push_constant(CommandBuffer& command, const DrawElement& element)
{
    struct PushConstant
    {
        uint32_t view_indices_offset;
    };

    command.push_constant<PushConstant>({
        .view_indices_offset = element.view_indices_offset,
    });
}

void DrawListSystem::bind_material_push_constant(CommandBuffer& command, const DrawElement& element)
{
    struct PushConstant
    {
        uint32_t view_indices_offset;
        uint32_t material_buffer_offset;
    };

    command.push_constant<PushConstant>({
        .view_indices_offset = element.view_indices_offset,
        .material_buffer_offset = element.material_buffer_offset,
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

void dispatch_draw_list(
    CommandBuffer& command,
    DrawListHandle handle,
    const DrawListRasterPassInfo& info,
    uint32_t view_count)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->dispatch_draw_list(command, handle, info, view_count);
}

} // namespace Mizu
