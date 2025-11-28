#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "render_core/rhi/pipeline.h"

namespace Mizu
{

// Forward declarations
class Mesh;
class Material;
class Shader;
class CommandBuffer;
class ResourceGroup;
struct Frustum;

struct DrawElement
{
    std::shared_ptr<Mesh> mesh = nullptr;
    std::shared_ptr<Material> material = nullptr;

    uint32_t instance_count = 0;
    size_t transform_offset = 0;
};

struct DrawBlock
{
    static constexpr size_t MAX_DRAW_ELEMENTS = 10;

    std::array<DrawElement, MAX_DRAW_ELEMENTS> elements{};
    size_t num_elements = 0;

    size_t pipeline_hash = 0;
    std::shared_ptr<Shader> vertex_shader = nullptr;
    std::shared_ptr<Shader> fragment_shader = nullptr;
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
    std::vector<std::shared_ptr<ResourceGroup>> resource_groups;
};

class DrawBlockManager
{
  public:
    DrawBlockManager() = default;

    DrawListHandle create_draw_list(DrawListType type, const Frustum& camera, std::vector<uint64_t>& indices);

    const DrawList& get_draw_list(DrawListHandle handle) const;

    void reset();

  private:
    static constexpr size_t MAX_DRAW_LISTS = 10;
    std::array<DrawList, MAX_DRAW_LISTS> m_draw_lists;
    std::atomic<size_t> m_num_draw_lists = 0;
};

} // namespace Mizu