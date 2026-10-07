#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>

#include "asset/asset_handle.h"
#include "base/containers/inplace_vector.h"

#include "render/resources/gpu_resource_types.h"
#include "render/scene/draw_class.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "resources/resource_event_stream.h"
#include "scene/draw_class_registry.h"

namespace Mizu
{

class AssetLoadSystem;
class BufferResource;
class GpuTexturePool;
class ImageResource;
class MaterialResidencySystem;
class MeshResidencySystem;

struct SceneDrawableInfo
{
    StaticMeshHandle static_mesh_handle{};
    TransformHandle transform_handle{};

    MeshAssetHandle mesh_handle{};
    MaterialAssetHandle material_handle{};

    GpuMeshResidentRecord gpu_mesh_record{};
    GpuMeshDrawPayload gpu_mesh_draw{};
    uint32_t material_buffer_offset = std::numeric_limits<uint32_t>::max();
    uint32_t transform_slot_index = std::numeric_limits<uint32_t>::max();

    DrawClassId class_id = INVALID_DRAW_CLASS_ID;
    DrawableFlags flags = DrawableFlags::None;
};

class SceneSystem
{
  public:
    SceneSystem(MeshResidencySystem& mesh_residency_system, MaterialResidencySystem& material_residency_system);

    void update(const ResourceEventStream& stream);

    std::span<const SceneDrawableInfo> get_drawables() const { return m_drawable_slots; }
    const DrawClassRegistry& get_draw_class_registry() const { return m_draw_class_registry; }

  private:
    static constexpr size_t INVALID_SLOT = std::numeric_limits<size_t>::max();
    static constexpr uint32_t INVALID_SLOT_U32 = std::numeric_limits<uint32_t>::max();

    struct DependencyChain
    {
        size_t prev = INVALID_SLOT;
        size_t next = INVALID_SLOT;
    };

    struct RenderableSlot
    {
        bool occupied = false;

        SceneDrawableInfo drawable_info{};

        // TODO: The members below should most likely be atomic

        bool mesh_resident = false;
        bool material_resident = false;

        DependencyChain mesh_dependency{};
        DependencyChain material_dependency{};

        bool drawable = false;
        size_t drawable_slot_index = INVALID_SLOT;
    };

    std::array<RenderableSlot, StaticMeshConfig::MaxNumHandles> m_slots{};
    inplace_vector<SceneDrawableInfo, StaticMeshConfig::MaxNumHandles> m_drawable_slots{};

    std::unordered_map<MeshAssetHandle, size_t> m_mesh_dependency_head_map{};
    std::unordered_map<MaterialAssetHandle, size_t> m_material_dependency_head_map{};

    DrawClassRegistry m_draw_class_registry{};

    MeshResidencySystem& m_mesh_residency_system;
    MaterialResidencySystem& m_material_residency_system;

    void consume_renderable_events(const ResourceEventStream& stream);
    void consume_mesh_residency_events(const ResourceEventStream& stream);
    void consume_material_residency_events(const ResourceEventStream& stream);

    void handle_renderable_create_event(const RenderableEvent& event);
    void handle_renderable_destroy_event(const RenderableEvent& event);

    void handle_mesh_residency_gpu_resident_event(const MeshResidencyEvent& event);
    void handle_material_residency_gpu_resident_event(const MaterialResidencyEvent& event);

    bool try_transition_to_drawable(size_t slot_idx);

    bool is_mesh_resident(const MeshAssetHandle& handle) const;
    bool is_material_resident(const MaterialAssetHandle& handle) const;

    size_t allocate_drawable_slot(SceneDrawableInfo info);
    void free_drawable_slot(size_t index);

    void link_mesh_dependency(const MeshAssetHandle& handle, DependencyChain& chain, size_t slot_idx);
    void link_material_dependency(const MaterialAssetHandle& handle, DependencyChain& chain, size_t slot_idx);
    void unlink_mesh_dependency(const MeshAssetHandle& handle, size_t slot_idx);
    void unlink_material_dependency(const MaterialAssetHandle& handle, size_t slot_idx);
};

} // namespace Mizu