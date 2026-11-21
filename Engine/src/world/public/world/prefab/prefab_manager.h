#pragma once

#include <unordered_map>
#include <vector>

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
    PrefabController(PrefabManager* owner, size_t id) : m_owner(owner), m_id(id) {}

    void update(TransformDynamicState ds);

    TransformDynamicState get_transform_ds() const;
    TransformDynamicState& edit_transform_ds();

    bool is_valid() const;

  private:
    PrefabManager* m_owner = nullptr;
    size_t m_id = 0;

    PrefabEntry* get_entry() const;

    friend class PrefabManager;
};

class MIZU_WORLD_API PrefabManager
{
  public:
    PrefabController create(PrefabAssetHandle handle, TransformDynamicState transform_ds);
    PrefabController create(PrefabAssetHandle handle, TransformHandle transform_handle);

    void destroy(PrefabController controller);

  private:
    std::unordered_map<size_t, PrefabEntry> m_prefab_entries_map{};

    PrefabController create_internal(
        PrefabAssetHandle handle,
        TransformHandle transform_handle,
        bool owns_transform_handle);

    PrefabEntry* find_entry(size_t id);

    friend class PrefabController;
};

} // namespace Mizu
