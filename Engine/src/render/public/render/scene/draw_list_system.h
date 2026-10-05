#pragma once

#include <cstdint>
#include <limits>
#include <optional>

#include "render/core/camera.h"
#include "render/scene/draw_list_raster_pass.h"
#include "render/scene/draw_list_system_types.h"

namespace Mizu
{

class CommandBuffer;
class FrameLinearAllocator;
class GpuMeshPool;
class RenderGraphBuilder;
class SceneSystem;

struct DrawListRequest
{
    DrawListRasterPass* raster_pass = nullptr;
    RenderGraphPassBuilder& pass_builder;

    RasterState state{};
    FramebufferInfo targets{};

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

void draw_list_system_init(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);
void draw_list_system_shutdown();

void draw_list_system_reset();
void draw_list_system_add_passes(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);
void draw_list_system_finalize(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);
void draw_list_system_prepare(FrameLinearAllocator& frame_allocator);

DrawListHandle create_draw_list(const DrawListRequest& request);
void dispatch_draw_list(CommandBuffer& command, DrawListHandle handle, const DrawListRasterPassInfo& info);

} // namespace Mizu
