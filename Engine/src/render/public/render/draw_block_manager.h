#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "render_core/rhi/pipeline.h"
#include "shader/shader_declaration.h"

#include "render/resources/gpu_resource_types.h"

namespace Mizu
{

// Forward declarations
class Mesh;
class SceneSystem;
class Shader;
class CommandBuffer;
struct Frustum;

struct DrawElement
{
    GpuMeshDrawPayload gpu_mesh_draw{};
    uint32_t material_buffer_offset = std::numeric_limits<uint32_t>::max();

    uint32_t instance_count = 0;
    size_t transform_offset = 0;
};

struct DrawBlock
{
    static constexpr size_t MAX_DRAW_ELEMENTS = 10;

    std::array<DrawElement, MAX_DRAW_ELEMENTS> elements{};
    size_t num_elements = 0;

    size_t pipeline_hash = 0;
    ShaderInstance vertex_instance;
    ShaderInstance fragment_instance;
};

enum class DrawListType
{
    Opaque,
    Transparent,
    Shadow,
};

struct DrawList
{
    static constexpr size_t MAX_DRAW_BLOCKS = 128;

    std::array<DrawBlock, MAX_DRAW_BLOCKS> blocks{};
    size_t num_blocks = 0;

    DrawListType list_type{};
};

struct InstanceTransformInfo
{
    glm::mat4 transform;
    glm::mat4 normal_matrix;
};

using DrawListHandle = size_t;

struct DrawListInfo
{
    DrawListHandle handle;
    GraphicsPipelineDescription pipeline_desc;
};

class DrawBlockManager
{
  public:
    DrawBlockManager(SceneSystem& scene_system);

    DrawListHandle create_draw_list(DrawListType type, const Frustum& camera, std::vector<uint64_t>& indices);

    const DrawList& get_draw_list(DrawListHandle handle) const;

    void reset();

  private:
    SceneSystem& m_scene_system;

    static constexpr size_t MAX_DRAW_LISTS = 10;
    std::array<DrawList, MAX_DRAW_LISTS> m_draw_lists;
    std::atomic<size_t> m_num_draw_lists = 0;
};

} // namespace Mizu
