#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "render/core/camera.h"
#include "render/scene/draw_list_raster_pass.h"
#include "render/scene/draw_list_system_types.h"
#include "render/systems/frame_linear_allocator.h"

namespace Mizu
{

class BufferResource;
class CommandBuffer;
class DescriptorSet;
class GpuMeshPool;
class Pipeline;
class RenderGraphBuilder;
class SceneSystem;
struct DrawElement;
struct GpuDrawData;

struct DrawListRequest
{
    DrawListRasterPass* raster_pass = nullptr;
    std::optional<Frustum> frustum{};
    FrustumMask frustum_mask{};
    uint32_t view_count = 1;
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

    DrawListHandle create_draw_list(const DrawListRequest& request);

    void compile_draw_lists();
    void add_compile_draw_lists_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);

    void dispatch_draw_list(CommandBuffer& command, DrawListHandle handle, const DrawListRasterPassInfo& info);

  private:
    SceneSystem& m_scene_system;
    GpuMeshPool& m_gpu_mesh_pool;

    std::unordered_map<size_t, DrawListHandle> m_draw_list_cache{};
    std::unordered_map<size_t, uint32_t> m_compile_list_cache{};

    struct DrawListRecord
    {
        DrawListRasterPass* raster_pass = nullptr;
        uint32_t view_count = 1;
        uint32_t compiled_draw_list_idx = std::numeric_limits<uint32_t>::max();

        // Gpu driven rendering.
        uint32_t gpu_driven_indirect_commands_element_offset = std::numeric_limits<uint32_t>::max();
        uint32_t gpu_driven_indirect_count_element_offset = std::numeric_limits<uint32_t>::max();
    };

    struct CompileListRecord
    {
        bool is_compiled = false;

        std::optional<Frustum> frustum{};
        FrustumMask frustum_mask{};

        uint32_t num_draw_elements = 0;
        uint32_t num_draw_data = 0;
        uint32_t draw_elements_offset = 0;

        FrameAllocation draw_data_allocation{};
    };

    std::atomic<uint32_t> m_num_draw_lists{0};
    std::atomic<uint32_t> m_num_compile_lists{0};

    static constexpr uint32_t MAX_NUM_DRAW_LISTS = 10;
    static constexpr uint32_t MAX_NUM_COMPILE_LISTS = 6;

    std::array<DrawListRecord, MAX_NUM_DRAW_LISTS> m_draw_list_records{};
    std::array<CompileListRecord, MAX_NUM_COMPILE_LISTS> m_compile_list_records{};

    // Keep without initialization ({} braces) so that we can keep `DrawElement` and `GpuDrawData` defined in the cpp.
    std::vector<DrawElement> m_draw_elements;
    std::vector<GpuDrawData> m_draw_data;

    bool m_gpu_driven_rendering_enabled = false;

    // TODO: SUPER UGLY, used in order to pass the buffers from the `add_compile_draw_lists_pass` function to the
    // dispatch. Think of a better way of doing this.
    BufferResource* m_gpu_indirect_command_buffer = nullptr;
    BufferResource* m_gpu_indirect_count_buffer = nullptr;
    BufferResource* m_gpu_draw_data_buffer = nullptr;

    void compile_draw_list_job(uint32_t compile_list_idx);

    void dispatch_draw_list_cpu(CommandBuffer& command, DrawListHandle handle, const DrawListRasterPassInfo& info);
    void dispatch_draw_list_gpu(CommandBuffer& command, DrawListHandle handle, const DrawListRasterPassInfo& info);

    void bind_resources(CommandBuffer& command, DrawListHandle handle, uint32_t set) const;
    void bind_draw_index_push_constant(CommandBuffer& command, uint32_t draw_index) const;
};

void draw_list_system_init(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);
void draw_list_system_shutdown();
void draw_list_system_compile_draw_lists();
void draw_list_system_add_compile_draw_lists_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);
void draw_list_system_build_frame_resources(FrameLinearAllocator& linear_allocator);
void draw_list_system_reset();

DrawListHandle create_draw_list(const DrawListRequest& request);
void dispatch_draw_list(CommandBuffer& command, DrawListHandle handle, const DrawListRasterPassInfo& info);

} // namespace Mizu
