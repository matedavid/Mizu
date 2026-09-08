#pragma once

#include <unordered_map>

#include "asset/asset_handle.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"

#include "mizu_world_module.h"

namespace Mizu
{

struct PrefabEntry
{
    size_t id;
    PrefabAssetHandle prefab_handle{};
    TransformHandle transform_handle{};
    std::vector<StaticMeshHandle> static_meshes{};

    bool owns_transform_handle = false;
};

class PrefabManager;

class MIZU_WORLD_API PrefabController
{
  public:
    PrefabController() = default;
    PrefabController(PrefabManager* owner, PrefabEntry* entry) : m_owner(owner), m_entry(entry) {}

    void update(TransformDynamicState ds);

    TransformDynamicState get_transform_ds() const;
    TransformDynamicState& edit_transform_ds();

    bool is_valid() const;

  private:
    PrefabManager* m_owner = nullptr;
    PrefabEntry* m_entry = nullptr;

    friend class PrefabManager;
};

class MIZU_WORLD_API PrefabManager
{
  public:
    PrefabController create(PrefabAssetHandle handle, TransformDynamicState transform_ds);
    PrefabController create(PrefabAssetHandle handle, TransformHandle transform_handle);

    void destroy(PrefabController controller);

  private:
    std::unordered_map<PrefabAssetHandle, PrefabEntry> m_prefab_entries_map{};

    PrefabController create_internal(
        PrefabAssetHandle handle,
        TransformHandle transform_handle,
        bool owns_transform_handle);

    bool is_entry_valid(PrefabEntry* entry) const;

    friend class PrefabController;
};

} // namespace Mizu
