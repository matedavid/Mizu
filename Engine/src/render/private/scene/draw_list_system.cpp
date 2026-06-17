#include "render/scene/draw_list_system.h"

#include <algorithm>
#include <span>

#include "asset/asset_handle.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/runtime.h"
#include "render_core/rhi/command_buffer.h"

#include "render/state_manager/static_mesh_state_manager.h"
#include "resources/gpu_pools.h"
#include "scene/scene_system.h"

namespace Mizu
{

static constexpr size_t CACHE_LINE = std::hardware_destructive_interference_size;
static constexpr size_t DRAW_ELEMENTS_STRIDE =
    (StaticMeshConfig::MaxNumHandles + CACHE_LINE - 1) / CACHE_LINE * CACHE_LINE;

DrawListSystem::DrawListSystem(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool)
    : m_scene_system(scene_system)
    , m_gpu_mesh_pool(gpu_mesh_pool)
{
    constexpr size_t DRAW_ELEMENTS_SIZE = MAX_NUM_DRAW_LISTS * DRAW_ELEMENTS_STRIDE;

    m_draw_elements.resize(DRAW_ELEMENTS_SIZE);
    m_view_indices.resize(DRAW_ELEMENTS_SIZE);
}

void DrawListSystem::reset()
{
    m_draw_list_cache.clear();

    m_num_draw_lists.store(0, std::memory_order_relaxed);

    for (DrawListRecord& list : m_draw_lists)
    {
        list = DrawListRecord{};
    }
}

DrawListHandle2 DrawListSystem::create_draw_list(const DrawListRequest& request)
{
    const auto cache_it = m_draw_list_cache.find(request);
    if (cache_it != m_draw_list_cache.end())
    {
        return cache_it->second;
    }

    const uint32_t draw_list_index = m_num_draw_lists.fetch_add(1, std::memory_order_relaxed);

    if (draw_list_index >= MAX_NUM_DRAW_LISTS)
    {
        m_num_draw_lists.fetch_sub(1, std::memory_order_relaxed);

        MIZU_ASSERT(false, "Exceeded maximum number of draw lists ({})", MAX_NUM_DRAW_LISTS);
        return DrawListHandle2{};
    }

    DrawListRecord& draw_list = m_draw_lists[draw_list_index];
    draw_list.request = request;

    const DrawListHandle2 handle{.index = draw_list_index};
    m_draw_list_cache.insert({request, handle});

    return handle;
}

void DrawListSystem::compile_draw_lists()
{
    const uint32_t num_draw_lists = m_num_draw_lists.load(std::memory_order_relaxed);
    if (num_draw_lists == 0)
        return;

    PendingBatch compile_batch = g_job_system->schedule_batch();

    for (uint32_t i = 0; i < num_draw_lists; ++i)
    {
        compile_batch.add(&DrawListSystem::compile_draw_list, this, DrawListHandle2{.index = i});
    }

    const JobHandle compile_job_handle = compile_batch.submit();
    g_job_system->wait_for(compile_job_handle);
}

void DrawListSystem::dispatch_draw_list(CommandBuffer& command, DrawListHandle2 handle)
{
    MIZU_ASSERT(handle.is_valid(), "Invalid draw list handle");
    MIZU_ASSERT(
        handle.index < m_num_draw_lists.load(std::memory_order_relaxed), "Draw list handle index is out of range");

    const DrawListRecord& record = m_draw_lists[handle.index];
    const CompiledDrawList& compiled = record.compiled;

    if (!compiled.is_compiled)
    {
        MIZU_LOG_ERROR("Draw list at index {} has not been compiled yet, skipping.", handle.index);
        return;
    }

    struct PushConstant
    {
        uint32_t view_indices_offset;
    } push_constant;

    const BufferResource& vertex_buffer = *m_gpu_mesh_pool.get_vertex_buffer();
    const BufferResource& index_buffer = *m_gpu_mesh_pool.get_index_buffer();

    command.bind_vertex_buffer(vertex_buffer);
    command.bind_index_buffer(index_buffer);

    const auto draw_elements_begin = m_draw_elements.begin() + compiled.draw_elements_offset;

    for (size_t i = 0; i < compiled.num_draw_elements; ++i)
    {
        const DrawElement& element = draw_elements_begin[i];

        push_constant = {
            .view_indices_offset = static_cast<uint32_t>(element.view_indices_offset),
        };
        command.push_constant(push_constant);

        command.draw_indexed(
            element.mesh_draw.index_count,
            element.mesh_draw.first_index,
            element.mesh_draw.first_vertex,
            element.instance_count,
            0);
    }
}

static size_t create_sort_key(MeshAssetHandle mesh_handle, MaterialAssetHandle material_handle)
{
    return hash_compute(mesh_handle.get_id(), material_handle.get_id());
}

void DrawListSystem::compile_draw_list(DrawListHandle2 handle)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(handle.is_valid(), "Invalid draw list handle");
    MIZU_ASSERT(
        handle.index < m_num_draw_lists.load(std::memory_order_relaxed), "Draw list handle index is out of range");

    DrawListRecord& record = m_draw_lists[handle.index];
    // TODO: const DrawListRequest& request = record.request;

    const std::span<const SceneDrawableInfo> drawables = m_scene_system.get_drawables();

    size_t num_draw_elements = 0;
    const size_t draw_elements_offset = handle.index * DRAW_ELEMENTS_STRIDE;

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

        // TODO: Culling

        const size_t sort_key = create_sort_key(drawable.mesh_handle, drawable.material_handle);

        m_draw_elements[draw_elements_offset + num_draw_elements] = DrawElement{
            .mesh_draw = drawable.gpu_mesh_draw,
            .instance_count = 1,
            .material_buffer_offset = drawable.material_buffer_offset,
            .transform_buffer_offset = drawable.transform_slot_index,
            .sort_key = sort_key,
        };

        num_draw_elements += 1;
    }

    auto begin = m_draw_elements.begin() + draw_elements_offset;
    auto end = begin + num_draw_elements;

    std::sort(begin, end, [](const DrawElement& a, const DrawElement& b) { return a.sort_key < b.sort_key; });

    size_t current_sort_key = begin[0].sort_key;
    size_t current_instance_offset = 0;
    size_t move_backwards_offset = 0;

    DrawElement& first = begin[0];

    m_view_indices[draw_elements_offset] = static_cast<uint32_t>(first.transform_buffer_offset);
    first.view_indices_offset = 0;

    for (size_t i = 1; i < num_draw_elements; ++i)
    {
        DrawElement& element = begin[i];

        m_view_indices[draw_elements_offset + i] = static_cast<uint32_t>(element.transform_buffer_offset);

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

    record.compiled = CompiledDrawList{
        .is_compiled = true,
        .num_draw_elements = num_draw_elements,
        .draw_elements_offset = draw_elements_offset,
    };
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

void draw_list_system_reset()
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    return s_draw_list_system->reset();
}

DrawListHandle2 create_draw_list(const DrawListRequest& request)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    return s_draw_list_system->create_draw_list(request);
}

void dispatch_draw_list(CommandBuffer& command, DrawListHandle2 handle)
{
    MIZU_ASSERT(s_draw_list_system != nullptr, "DrawListSystem has not been initialized");
    s_draw_list_system->dispatch_draw_list(command, handle);
}

} // namespace Mizu