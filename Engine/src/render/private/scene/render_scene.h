#pragma once

#include <array>

#include "base/math/shapes.h"

#include "render/resources/gpu_resource_types.h"
#include "render/scene/draw_class.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "scene/draw_class_registry.h"

namespace Mizu
{

class MaterialSystem;
class MeshStreamingManager;
class StreamingPlanner;
struct MaterialUpdate;
struct MeshStreamingUpdate;

constexpr size_t INVALID_SLOT = std::numeric_limits<size_t>::max();

struct SceneDrawable
{
    size_t owner_slot = INVALID_SLOT;

    MeshAssetHandle mesh_handle{};
    MaterialAssetHandle material_handle{};

    GpuMeshDrawPayload gpu_mesh_draw{};
    math::AABB bounding_box{};

    uint32_t transform_slot_index = std::numeric_limits<uint32_t>::max();
    uint32_t material_buffer_offset = std::numeric_limits<uint32_t>::max();

    DrawClassId class_id = INVALID_DRAW_CLASS_ID;
    DrawableFlags flags = DrawableFlags::None;
};

class RenderScene : public StaticMeshStateManagerConsumer
{
  public:
    RenderScene(StreamingPlanner& streaming_planner, MaterialSystem& material_system);
    ~RenderScene() override;

    void update();

    std::span<const SceneDrawable> get_drawables() const { return m_drawables; }
    const DrawClassRegistry& get_draw_class_registry() const { return m_draw_class_registry; }

    // StaticMeshStateManagerConsumer
    void rend_on_create(StaticMeshHandle handle, const StaticMeshStaticState& ss, const StaticMeshDynamicState& ds)
        override;
    void rend_on_update(StaticMeshHandle handle, const StaticMeshDynamicState& ds) override;
    void rend_on_destroy(StaticMeshHandle handle) override;

  private:
    StreamingPlanner& m_streaming_planner;
    MeshStreamingManager& m_mesh_streaming_manager;
    MaterialSystem& m_material_system;

    DrawClassRegistry m_draw_class_registry{};

    struct DependencyChain
    {
        size_t prev = INVALID_SLOT;
        size_t next = INVALID_SLOT;
    };

    struct RenderableSlot
    {
        bool occupied = false;
        size_t slot_index = INVALID_SLOT;

        StaticMeshHandle static_mesh_handle{};

        StaticMeshStaticState ss{};

        // Point to the prev and next RenderableSlot that uses a given mesh/material
        DependencyChain mesh_dependency_chain{};
        DependencyChain material_dependency_chain{};

        // TODO: The members below should most likely be atomic

        bool is_mesh_drawable = false;
        bool is_material_drawable = false;

        bool is_drawable = false;
        size_t drawable_slot_index = INVALID_SLOT;
    };

    std::array<RenderableSlot, StaticMeshConfig::MaxNumHandles> m_slots{};
    inplace_vector<SceneDrawable, StaticMeshConfig::MaxNumHandles> m_drawables{}; // packed array of drawables

    // Point to a RenderableSlot, the head of the chain
    std::unordered_map<MeshAssetHandle, size_t> m_mesh_dependency_chain_head_map{};
    std::unordered_map<MaterialAssetHandle, size_t> m_material_dependency_head_map{};

    void handle_mesh_update(const MeshStreamingUpdate& update);
    void handle_material_update(const MaterialUpdate& update);
    bool try_transition_to_drawable(RenderableSlot& slot);

    bool is_mesh_drawable(MeshAssetHandle handle) const;
    bool is_material_drawable(MaterialAssetHandle handle) const;

    size_t allocate_drawable_slot(const SceneDrawable& info);
    void free_drawable_slot(size_t index);

    // Returns the new next index
    template <AssetHandleType AssetHandleT>
    void add_dependency(
        std::unordered_map<AssetHandleT, size_t>& dependency_head_map,
        AssetHandleT handle,
        DependencyChain RenderableSlot::* chain_member,
        size_t new_slot);
    template <AssetHandleType AssetHandleT>
    void remove_dependency(
        std::unordered_map<AssetHandleT, size_t>& dependency_head_map,
        AssetHandleT handle,
        DependencyChain RenderableSlot::* chain_member,
        size_t removing_slot);
};

} // namespace Mizu