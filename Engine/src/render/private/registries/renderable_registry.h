#pragma once

#include <array>
#include <span>

#include "base/containers/inplace_vector.h"

#include "render/state_manager/static_mesh_state_manager.h"

namespace Mizu
{

struct RenderableRegistryEntry
{
    StaticMeshHandle static_mesh_handle;
    TransformHandle transform_handle;

    MeshAssetHandle mesh_handle;
    MaterialAssetHandle material_handle;
};

struct RenderableRegistryDelta
{
    enum class Type
    {
        Create,
        Update,
        Destroy,
    };

    Type type;
    StaticMeshHandle handle;

    MeshAssetHandle mesh_handle;
    MaterialAssetHandle material_handle;
};

class RenderableRegistry : public StaticMeshStateManagerConsumer
{
  public:
    RenderableRegistry();
    ~RenderableRegistry() override;

    void update(ResourceEventStream& stream);

    std::span<const RenderableRegistryEntry> get_entries() const { return m_entries; }

    void rend_on_create(StaticMeshHandle handle, const StaticMeshStaticState& ss, const StaticMeshDynamicState& ds)
        override;
    void rend_on_update(StaticMeshHandle handle, const StaticMeshDynamicState& ds) override;
    void rend_on_destroy(StaticMeshHandle handle) override;

  private:
    inplace_vector<RenderableRegistryEntry, StaticMeshConfig::MaxNumHandles> m_entries{};

    std::array<RenderableRegistryDelta, StaticMeshConfig::MaxNumHandles> m_deltas{};
    size_t m_deltas_head = 0;
    size_t m_deltas_tail = 0;
    size_t m_deltas_size = 0;

    void add_delta(RenderableRegistryDelta delta);
    bool consume_delta(RenderableRegistryDelta& delta);
};

void renderable_registry_init();
void renderable_registry_shutdown();
RenderableRegistry& renderable_registry_get();
void renderable_registry_update(ResourceEventStream& stream);

} // namespace Mizu