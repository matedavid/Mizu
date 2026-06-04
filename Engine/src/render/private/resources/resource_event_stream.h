#pragma once

#include <span>

#include "asset/asset_handle.h"
#include "base/containers/inplace_vector.h"

#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

enum class RenderableEventType
{
    Create,
    Update,
    Destroy,
};

struct RenderableEvent
{
    RenderableEventType type{};
    TransformHandle transform_handle{};
    StaticMeshHandle static_mesh_handle{};
    MeshAssetHandle mesh_handle{};
    MaterialAssetHandle material_handle{};
};

enum class ResidencySystemEventType
{
    Loading,
    GpuResident,
    Evicting,
};

struct MeshResidencyEvent
{
    ResidencySystemEventType type{};
    MeshAssetHandle mesh_handle{};
};

struct TextureResidencyEvent
{
    ResidencySystemEventType type{};
    TextureAssetHandle texture_handle{};
};

struct MaterialResidencyEvent
{
    ResidencySystemEventType type{};
    MaterialAssetHandle material_handle{};
};

class ResourceEventStream
{
  public:
    ResourceEventStream() = default;

    void reset();

    void push_renderable_event(const RenderableEvent& event);
    void push_mesh_residency_event(const MeshResidencyEvent& event);
    void push_texture_residency_event(const TextureResidencyEvent& event);
    void push_material_residency_event(const MaterialResidencyEvent& event);

    std::span<const RenderableEvent> get_renderable_events() const { return m_renderable_events; }
    std::span<const MeshResidencyEvent> get_mesh_residency_events() const { return m_mesh_residency_events; }
    std::span<const MaterialResidencyEvent> get_material_residency_events() const
    {
        return m_material_residency_events;
    }

  private:
    static constexpr size_t MAX_EVENTS = 1024;

    inplace_vector<RenderableEvent, MAX_EVENTS> m_renderable_events{};
    inplace_vector<MeshResidencyEvent, MAX_EVENTS> m_mesh_residency_events{};
    inplace_vector<TextureResidencyEvent, MAX_EVENTS> m_texture_residency_events{};
    inplace_vector<MaterialResidencyEvent, MAX_EVENTS> m_material_residency_events{};
};

} // namespace Mizu