#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "render_core/rhi/buffer_resource.h"

#include "render/render_graph/render_graph_builder.h"
#include "render/scene/draw_list_system.h"
#include "render/scene/draw_list_system_types.h"
#include "render/systems/frame_linear_allocator.h"

namespace Mizu
{

class CommandBuffer;
class GpuMeshPool;
class Pipeline;
class SceneSystem;

inline constexpr uint32_t INVALID_BUCKET_INDEX = std::numeric_limits<uint32_t>::max();
inline constexpr uint32_t INVALID_BUCKET_SLOT = std::numeric_limits<uint32_t>::max();

// Must match GpuDrawData in gpu_driven_rendering.slang
struct GpuDrawData
{
    uint32_t transform_slot;
    uint32_t material_offset;
};

// Must match GpuDrawableInstance in compile_draw_lists.slang
struct GpuDrawableInstance
{
    glm::vec3 aabb_min;
    uint32_t transform_slot;
    glm::vec3 aabb_max;
    uint32_t material_offset;

    uint32_t index_count;
    uint32_t first_index;
    uint32_t first_vertex;
    uint32_t class_and_flags;
};

// Must match GpuVisibilityInfo in compile_draw_lists.slang
struct GpuVisibilityInfo
{
    glm::vec4 planes[6];
    uint32_t frustum_mask;
    uint32_t draw_list_begin;
    uint32_t draw_list_count;
    uint32_t _pad;
};

// Must match GpuDrawListInfo in compile_draw_lists.slang
struct GpuDrawListInfo
{
    uint32_t bucket_map_offset;
    uint32_t view_count;
    uint32_t filter_required;
    uint32_t filter_excluded;
};

// Must match GpuBucketInfo in compile_draw_lists.slang
struct GpuBucketInfo
{
    uint32_t region_base;
    uint32_t capacity;
};

struct DrawBucket
{
    RasterShaders shaders{};
    std::shared_ptr<Pipeline> pipeline{};

    // Cpu backend
    uint32_t element_first = 0;
    uint32_t element_count = 0;

    // Gpu backend
    uint32_t slot = INVALID_BUCKET_SLOT;
    uint32_t region_base = 0;
    uint32_t capacity = 0;
};

struct DrawElement
{
    uint32_t index_count = 0;
    uint32_t first_index = 0;
    uint32_t first_vertex = 0;
    uint32_t instance_count = 0;
    uint32_t draw_index = 0;

#if MIZU_DEBUG
    std::string_view debug_name{};
#endif
};

struct VisibilityEntry
{
    struct DrawClassRange
    {
        uint32_t class_id = 0;

        // Both index into `visible`
        uint32_t begin = 0;
        uint32_t end = 0;
    };

    VisibilityDesc desc{};

    std::vector<uint32_t> visible{};

    // Cpu backend
    std::vector<DrawClassRange> ranges{};

    // Gpu backend
    std::vector<uint32_t> draw_lists{};
    uint32_t list_begin = 0;
};

struct DrawListEntry
{
    DrawListKey key{};

    const DrawListRasterPass* raster_pass = nullptr;
    RasterState state{};
    FramebufferInfo targets{};
    DrawFilter filter{};
    uint32_t view_count = 1;
    uint32_t visibility_idx = 0;

    // Indexed by DrawClassId, INVALID_BUCKET_INDEX when the pass does not draw that class
    std::vector<uint32_t> class_to_bucket{};
    std::vector<DrawBucket> buckets{};

    // Cpu backend
    std::vector<DrawElement> elements{};
    std::vector<GpuDrawData> draw_data{};
    FrameAllocation draw_data_allocation{};

    // Gpu backend
    uint32_t bucket_map_offset = 0;
};

class DrawListSystem
{
  public:
    DrawListSystem(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);

    void reset();

    DrawListHandle create_draw_list(const DrawListRequest& request);

    void add_passes(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);
    void finalize(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);
    void prepare(FrameLinearAllocator& frame_allocator);

    void dispatch_draw_list(CommandBuffer& command, DrawListHandle handle, const DrawListRasterPassInfo& info);

  private:
    SceneSystem& m_scene_system;
    GpuMeshPool& m_gpu_mesh_pool;

    bool m_gpu_driven_rendering_enabled = false;

    std::vector<VisibilityEntry> m_visibilities{};
    std::vector<DrawListEntry> m_draw_lists{};

    uint32_t m_num_visibilities = 0;
    uint32_t m_num_draw_lists = 0;

    std::unordered_multimap<size_t, uint32_t> m_visibility_map{};
    std::unordered_multimap<size_t, uint32_t> m_draw_list_map{};

    struct GpuResources
    {
        bool valid = false;

        uint32_t num_drawables = 0;
        uint32_t num_bucket_slots = 0;
        uint32_t total_capacity = 0;

        RenderGraphResource indirect_commands_buffer{};
        RenderGraphResource count_buffer{};
        RenderGraphResource draw_data_buffer{};

        FrameAllocation instances{};
        FrameAllocation visibilities{};
        FrameAllocation draw_lists{};
        FrameAllocation visibility_lists{};
        FrameAllocation bucket_map{};
        FrameAllocation buckets{};

        BufferResource* resolved_indirect_commands_buffer = nullptr;
        BufferResource* resolved_count_buffer = nullptr;
        BufferResource* resolved_draw_data_buffer = nullptr;
    };

    GpuResources m_gpu_resources{};

    uint32_t get_visibility_id(const VisibilityDesc& desc);

    // Cpu backend
    void cpu_visibility_job(uint32_t visibility_idx);
    void cpu_build_list_job(uint32_t list_idx);
    void cpu_upload_draw_data(FrameLinearAllocator& frame_allocator);
    void cpu_dispatch_bucket(CommandBuffer& command, const DrawListEntry& list, const DrawBucket& bucket) const;

    // Gpu backend
    void gpu_add_passes(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);
    void gpu_add_culling_and_generate_pass(RenderGraphBuilder& builder);
    void gpu_build_tables(FrameLinearAllocator& frame_allocator);
    void gpu_dispatch_bucket(CommandBuffer& command, const DrawBucket& bucket) const;

    // Execution
    std::shared_ptr<DescriptorSet> create_draw_list_descriptor_set(const DrawListEntry& list) const;
    void resolve_pipelines();
};

} // namespace Mizu
