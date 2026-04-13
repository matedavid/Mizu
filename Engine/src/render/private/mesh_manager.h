#pragma once

#include <span>

#include "base/containers/inplace_vector.h"

#include "render/material/material.h"
#include "render/model/mesh.h"
#include "render/state_manager/static_mesh_state_manager.h"

namespace Mizu
{

struct MeshManagerEntry
{
    StaticMeshHandle handle;
    TransformHandle transform_handle;

    TransformDynamicState transform_ds;

    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
};

class MeshManager : public StaticMeshStateManagerConsumer
{
  public:
    MeshManager();
    ~MeshManager() override;

    MeshManager(const MeshManager&) = delete;
    MeshManager& operator=(const MeshManager&) = delete;

    void update();
    std::span<const MeshManagerEntry> get_meshes() const;

    void rend_on_create(StaticMeshHandle handle, const StaticMeshStaticState& ss, const StaticMeshDynamicState& ds)
        override;
    void rend_on_update(StaticMeshHandle handle, const StaticMeshDynamicState& ds) override;
    void rend_on_destroy(StaticMeshHandle handle) override;

  private:
    inplace_vector<MeshManagerEntry, StaticMeshConfig2::MaxNumHandles> m_meshes;
};

void mesh_manager_init();
void mesh_manager_shutdown();
void mesh_manager_update();
const MeshManager& mesh_manager_get();

} // namespace Mizu
