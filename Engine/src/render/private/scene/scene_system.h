#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stack>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"
#include "base/containers/inplace_vector.h"

#include "render/render_graph/render_graph_builder.h"
#include "render/resources/gpu_resource_types.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"
#include "render/systems/frame_linear_allocator.h"
#include "resources/resource_event_stream.h"

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
};

class SceneSystem : public TransformStateManagerConsumer
{
  public:
    SceneSystem(MeshResidencySystem& mesh_residency_system, MaterialResidencySystem& material_residency_system);
    ~SceneSystem() override;

    void update(const ResourceEventStream& stream, uint64_t frame_num);
    void add_transform_publish_pass(RenderGraphBuilder& builder, FrameLinearAllocator& linear_allocator);

    std::span<const SceneDrawableInfo> get_drawables() const { return m_drawable_slots; }
    std::shared_ptr<BufferResource> get_transform_info_buffer() const { return m_transform_info_buffer; }

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

    std::vector<TransformInfo> m_transform_infos{};
    std::array<uint32_t, TransformConfig::MaxNumHandles> m_transform_slot_indices{};
    std::stack<uint32_t> m_free_transform_slots{};

    struct PendingTransformUpdate
    {
        TransformInfo new_transform{};
        uint32_t dst_slot = INVALID_SLOT_U32;

        uint32_t _padding[3] = {};
    };

    struct PendingTransformEviction
    {
        uint32_t slot_idx = INVALID_SLOT_U32;
        uint64_t last_frame_num = 0;
    };

    std::vector<PendingTransformUpdate> m_pending_transform_updates{};
    std::vector<PendingTransformEviction> m_pending_transform_evictions{};

    std::shared_ptr<BufferResource> m_transform_info_buffer{};

    std::unordered_map<MeshAssetHandle, size_t> m_mesh_dependency_head_map{};
    std::unordered_map<MaterialAssetHandle, size_t> m_material_dependency_head_map{};

    MeshResidencySystem& m_mesh_residency_system;
    MaterialResidencySystem& m_material_residency_system;

    void consume_renderable_events(const ResourceEventStream& stream, uint64_t frame_num);
    void consume_mesh_residency_events(const ResourceEventStream& stream);
    void consume_material_residency_events(const ResourceEventStream& stream);
    void track_transform_evictions(uint64_t frame_num);

    void handle_renderable_create_event(const RenderableEvent& event);
    void handle_renderable_destroy_event(const RenderableEvent& event, uint64_t frame_num);

    void handle_mesh_residency_gpu_resident_event(const MeshResidencyEvent& event);
    void handle_material_residency_gpu_resident_event(const MaterialResidencyEvent& event);

    bool try_transition_to_drawable(size_t slot_idx);

    bool is_mesh_resident(const MeshAssetHandle& handle) const;
    bool is_material_resident(const MaterialAssetHandle& handle) const;

    size_t allocate_drawable_slot(SceneDrawableInfo info);
    void free_drawable_slot(size_t index);

    uint32_t allocate_transform_slot(const TransformHandle& handle);
    void free_transform_slot(uint32_t slot);

    void rend_on_create(TransformHandle, const TransformStaticState&, const TransformDynamicState&) override {}
    void rend_on_update(TransformHandle handle, const TransformDynamicState& ds) override;
    void rend_on_destroy(TransformHandle) override {}

    TransformInfo build_transform_info(const TransformDynamicState& ds);

    void link_mesh_dependency(const MeshAssetHandle& handle, DependencyChain& chain, size_t slot_idx);
    void link_material_dependency(const MaterialAssetHandle& handle, DependencyChain& chain, size_t slot_idx);
    void unlink_mesh_dependency(const MeshAssetHandle& handle, size_t slot_idx);
    void unlink_material_dependency(const MaterialAssetHandle& handle, size_t slot_idx);
};

} // namespace Mizu