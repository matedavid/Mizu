#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

#include "render/core/camera.h"
#include "render/frame_linear_allocator.h"
#include "render/scene/draw_list_raster_pass.h"
#include "render/scene/draw_list_system_types.h"

namespace Mizu
{

class CommandBuffer;
class DescriptorSet;
class GpuMeshPool;
class Pipeline;
class SceneSystem;
struct DrawElement;

struct DrawListRequest
{
    DrawListRasterPass* raster_pass = nullptr;
    std::optional<Frustum> frustum{};
    FrustumMask frustum_mask{};
};

struct DrawListHandle
{
    static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

    uint32_t index = INVALID_INDEX;

    bool is_valid() const { return index != INVALID_INDEX; }
};

class DrawListSystem
{
  public:
    DrawListSystem(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);
    ~DrawListSystem();

    void reset();
    void build_frame_resources(FrameLinearAllocator& linear_allocator);

    void bind_resources(CommandBuffer& command, DrawListHandle handle, uint32_t set);

    DrawListHandle create_draw_list(const DrawListRequest& request);
    void compile_draw_lists();
    void dispatch_draw_list(
        CommandBuffer& command,
        DrawListHandle handle,
        const DrawListRasterPassInfo& info,
        uint32_t view_count);

  private:
    SceneSystem& m_scene_system;
    GpuMeshPool& m_gpu_mesh_pool;

    std::unordered_map<size_t, DrawListHandle> m_draw_list_cache{};
    std::unordered_map<size_t, uint32_t> m_compile_list_cache{};

    struct DrawListRecord
    {
        DrawListRasterPass* raster_pass = nullptr;
        uint32_t compiled_draw_list_idx = std::numeric_limits<uint32_t>::max();
    };

    struct CompileListRecord
    {
        bool is_compiled = false;

        std::optional<Frustum> frustum{};
        FrustumMask frustum_mask{};

        uint32_t num_draw_elements = 0;
        uint32_t num_view_indices = 0;
        uint32_t draw_elements_offset = 0;

        FrameAllocation view_indices_allocation{};
    };

    std::atomic<uint32_t> m_num_draw_lists{0};
    std::atomic<uint32_t> m_num_compile_lists{0};

    static constexpr uint32_t MAX_NUM_DRAW_LISTS = 10;
    static constexpr uint32_t MAX_NUM_COMPILE_LISTS = 6;

    std::array<DrawListRecord, MAX_NUM_DRAW_LISTS> m_draw_list_records{};
    std::array<CompileListRecord, MAX_NUM_COMPILE_LISTS> m_compile_list_records{};

    // Keep `m_draw_elements` without initialization ({} braces) so that we can keep `DrawElement` defined in the cpp.
    std::vector<DrawElement> m_draw_elements;
    std::vector<uint32_t> m_view_indices{};

    void compile_draw_list_job(uint32_t compile_list_idx);

    void bind_default_push_constant(CommandBuffer& command, const DrawElement& element);
    void bind_material_push_constant(CommandBuffer& command, const DrawElement& element);
};

void draw_list_system_init(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);
void draw_list_system_shutdown();
void draw_list_system_compile_draw_lists();
void draw_list_system_build_frame_resources(FrameLinearAllocator& linear_allocator);
void draw_list_system_reset();

DrawListHandle create_draw_list(const DrawListRequest& request);
void dispatch_draw_list(
    CommandBuffer& command,
    DrawListHandle handle,
    const DrawListRasterPassInfo& info,
    uint32_t view_count = 1);

} // namespace Mizu
