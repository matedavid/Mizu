#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/debug/assert.h"
#include "base/utils/hash.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/rhi/render_pass.h"
#include "shader/shader_declaration.h"

#include "render/core/camera.h"
#include "render/frame_linear_allocator.h"
#include "render/resources/gpu_resource_types.h"

namespace Mizu
{

class CommandBuffer;
class DescriptorSet;
class GpuMeshPool;
class Pipeline;
class SceneSystem;

enum class DrawListKind
{
    DepthOnly,
    Material,
};

struct DrawListRequest
{
    std::optional<Frustum> frustum{};
    FrustumMask frustum_mask{};
};

struct DrawListRasterBindings
{
    std::array<std::shared_ptr<DescriptorSet>, MAX_DESCRIPTOR_SET_COUNT> descriptor_sets{};

    DrawListRasterBindings& add(uint32_t set, std::shared_ptr<DescriptorSet> descriptor_set)
    {
        MIZU_ASSERT(
            set < MAX_DESCRIPTOR_SET_COUNT,
            "Set is higher than the max descriptor set count ({} >= {})",
            set,
            MAX_DESCRIPTOR_SET_COUNT);

        if (descriptor_sets[set] != nullptr)
        {
            MIZU_ASSERT(false, "Already added descriptor set at set {}", set);
            return *this;
        }

        descriptor_sets[set] = std::move(descriptor_set);
        return *this;
    }
};

struct DrawListRasterPass
{
    RasterizationState rasterization_state{};
    DepthStencilState depth_stencil_state{};
    ColorBlendState color_blend_state{};
    FramebufferInfo framebuffer_info{};

    DrawListKind draw_list_kind = DrawListKind::DepthOnly;
    DrawListRasterBindings bindings{};
};

struct DrawListHandle2
{
    static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

    uint32_t index = INVALID_INDEX;

    bool is_valid() const { return index != INVALID_INDEX; }
};

class DrawListSystem
{
  public:
    DrawListSystem(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);

    void reset();
    void build_frame_resources(FrameLinearAllocator& linear_allocator);

    void bind_resources(CommandBuffer& command, DrawListHandle2 handle, uint32_t set);

    DrawListHandle2 create_draw_list(const DrawListRequest& request);
    void compile_draw_lists();
    void dispatch_draw_list(
        CommandBuffer& command,
        DrawListHandle2 handle,
        const DrawListRasterPass& raster_pass,
        uint32_t view_count);

  private:
    SceneSystem& m_scene_system;
    GpuMeshPool& m_gpu_mesh_pool;

    struct DrawListRequestHash
    {
        size_t operator()(const DrawListRequest& request) const
        {
            size_t h = 0;

            // Don't add draw value as it's more of a dispatch argument, not a unique identifier for the draw list.

            hash_combine(
                h,
                request.frustum_mask.top,
                request.frustum_mask.bottom,
                request.frustum_mask.left,
                request.frustum_mask.right,
                request.frustum_mask.near,
                request.frustum_mask.far);

            hash_combine(h, request.frustum.has_value());

            if (request.frustum.has_value())
            {
                const Frustum& frustum = *request.frustum;

                hash_combine(h, frustum.center.x, frustum.center.y, frustum.center.z);
                hash_combine(h, frustum.near.distance, frustum.far.distance);
            }

            return h;
        }
    };

    struct DrawListRequestEqual
    {
        bool operator()(const DrawListRequest& lhs, const DrawListRequest& rhs) const
        {
            // Don't add draw value as it's more of a dispatch argument, not a unique identifier for the draw list.

            if (lhs.frustum_mask != rhs.frustum_mask)
                return false;

            if (lhs.frustum.has_value() != rhs.frustum.has_value())
                return false;

            if (lhs.frustum.has_value())
                return *lhs.frustum == *rhs.frustum;

            return true;
        }
    };

    std::unordered_map<DrawListRequest, DrawListHandle2, DrawListRequestHash, DrawListRequestEqual> m_draw_list_cache{};

    struct CompiledDrawList
    {
        bool is_compiled = false;

        uint32_t num_draw_elements = 0;
        uint32_t num_view_indices = 0;
        uint32_t draw_elements_offset = 0;

        FrameAllocation view_indices_allocation{};
    };

    struct DrawListRecord
    {
        DrawListRequest request{};
        CompiledDrawList compiled{};
    };

    std::atomic<uint32_t> m_num_draw_lists{0};

    static constexpr uint32_t MAX_NUM_DRAW_LISTS = 10;
    std::array<DrawListRecord, MAX_NUM_DRAW_LISTS> m_draw_lists{};

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
    };

    std::vector<DrawElement> m_draw_elements{};
    std::vector<uint32_t> m_view_indices{};

    void compile_draw_list_job(DrawListHandle2 handle);

    void bind_default_push_constant(CommandBuffer& command, const DrawElement& element);
    void bind_material_push_constant(CommandBuffer& command, const DrawElement& element);
};

void draw_list_system_init(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);
void draw_list_system_shutdown();
void draw_list_system_compile_draw_lists();
void draw_list_system_build_frame_resources(FrameLinearAllocator& linear_allocator);
void draw_list_system_reset();

DrawListHandle2 create_draw_list(const DrawListRequest& request);
void dispatch_draw_list(
    CommandBuffer& command,
    DrawListHandle2 handle,
    const DrawListRasterPass& raster_pass,
    uint32_t view_count = 1);

} // namespace Mizu
