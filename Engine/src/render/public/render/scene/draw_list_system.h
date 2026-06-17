#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/utils/hash.h"
#include "render/resources/gpu_resource_types.h"

namespace Mizu
{

class CommandBuffer;
class GpuMeshPool;
class SceneSystem;

enum class DrawListKind
{
    Opaque,
    DepthOnly,
    Transparent,
};

struct DrawListRequest
{
    DrawListKind draw_kind;
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

    DrawListHandle2 create_draw_list(const DrawListRequest& request);
    void compile_draw_lists();
    void dispatch_draw_list(CommandBuffer& command, DrawListHandle2 handle);

  private:
    SceneSystem& m_scene_system;
    GpuMeshPool& m_gpu_mesh_pool;

    struct DrawListRequestHash
    {
        size_t operator()(const DrawListRequest& request) const
        {
            size_t h = 0;

            hash_combine(h, request.draw_kind);

            return h;
        }
    };

    struct DrawListRequestEqual
    {
        bool operator()(const DrawListRequest& lhs, const DrawListRequest& rhs) const
        {
            return lhs.draw_kind == rhs.draw_kind;
        }
    };

    std::unordered_map<DrawListRequest, DrawListHandle2, DrawListRequestHash, DrawListRequestEqual> m_draw_list_cache{};

    struct CompiledDrawList
    {
        bool is_compiled = false;

        size_t num_draw_elements = 0;
        size_t draw_elements_offset = 0;
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
        uint32_t instance_count = std::numeric_limits<uint32_t>::max();
        uint32_t material_buffer_offset = std::numeric_limits<uint32_t>::max();
        size_t transform_buffer_offset = std::numeric_limits<size_t>::max();
        size_t view_indices_offset = std::numeric_limits<size_t>::max();

        size_t sort_key = 0;
    };

    std::vector<DrawElement> m_draw_elements{};
    std::vector<uint32_t> m_view_indices{};

    void compile_draw_list(DrawListHandle2 handle);
};

void draw_list_system_init(SceneSystem& scene_system, GpuMeshPool& gpu_mesh_pool);
void draw_list_system_shutdown();
void draw_list_system_compile_draw_lists();
void draw_list_system_reset();

DrawListHandle2 create_draw_list(const DrawListRequest& request);
void dispatch_draw_list(CommandBuffer& command, DrawListHandle2 handle);

} // namespace Mizu
