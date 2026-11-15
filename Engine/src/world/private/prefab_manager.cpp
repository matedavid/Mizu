#include "world/prefab/prefab_manager.h"

#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/types/uuid.h"
#include "core/game_context.h"

namespace Mizu
{

//
// PrefabController
//

void PrefabController::update(TransformDynamicState ds)
{
    const PrefabEntry* entry = get_entry();
    if (entry == nullptr)
    {
        MIZU_ASSERT(false, "Invalid PrefabController");
        return;
    }

    g_transform_state_manager->sim_update(entry->transform_handle, ds);
}

TransformDynamicState PrefabController::get_transform_ds() const
{
    const PrefabEntry* entry = get_entry();
    if (entry == nullptr)
    {
        MIZU_ASSERT(false, "Invalid PrefabController");
        return TransformDynamicState{};
    }

    return g_transform_state_manager->sim_get_dynamic_state(entry->transform_handle);
}

TransformDynamicState& PrefabController::edit_transform_ds()
{
    const PrefabEntry* entry = get_entry();
    if (entry == nullptr)
    {
        MIZU_ASSERT(false, "Invalid PrefabController");

        static TransformDynamicState s_invalid_transform_ds{};
        s_invalid_transform_ds = TransformDynamicState{};
        return s_invalid_transform_ds;
    }

    return g_transform_state_manager->sim_edit(entry->transform_handle);
}

bool PrefabController::is_valid() const
{
    return get_entry() != nullptr;
}

PrefabEntry* PrefabController::get_entry() const
{
    if (m_owner == nullptr)
        return nullptr;

    return m_owner->find_entry(m_id);
}

//
// PrefabManager
//

PrefabController PrefabManager::create(PrefabAssetHandle handle, TransformDynamicState transform_ds)
{
    const TransformHandle transform_handle = g_transform_state_manager->sim_create({}, transform_ds);
    return create_internal(handle, transform_handle, true);
}

PrefabController PrefabManager::create(PrefabAssetHandle handle, TransformHandle transform_handle)
{
    return create_internal(handle, transform_handle, false);
}

void PrefabManager::destroy(PrefabController controller)
{
    auto it = m_prefab_entries_map.find(controller.m_id);
    if (it == m_prefab_entries_map.end())
    {
        MIZU_ASSERT(false, "Trying to destroy invalid PrefabController");
        return;
    }

    PrefabEntry& entry = it->second;

    if (entry.owns_transform_handle)
    {
        g_transform_state_manager->sim_destroy(entry.transform_handle);
    }

    for (const StaticMeshHandle& mesh_handle : entry.static_meshes)
    {
        g_static_mesh_state_manager->sim_destroy(mesh_handle);
    }

    m_prefab_entries_map.erase(it);
}

PrefabController PrefabManager::create_internal(
    PrefabAssetHandle handle,
    TransformHandle transform_handle,
    bool owns_transform_handle)
{
    PrefabController invalid_controller{this, 0};

    IAssetLoader& asset_loader = g_game_context->get_asset_loader();

    const std::optional<PrefabAssetRecord> record = asset_loader.get_prefab_record(handle);
    if (!record.has_value())
    {
        MIZU_ASSERT(false, "Failed to get record for prefab: {}", handle.get_id());
        return invalid_controller;
    }

    PrefabEntry entry{};
    entry.id = static_cast<size_t>(UUID{});
    entry.prefab_handle = handle;
    entry.transform_handle = transform_handle;
    entry.static_meshes.reserve(record->metadata.num_meshes);
    entry.owns_transform_handle = owns_transform_handle;

    std::vector<uint8_t> payload(record->metadata.get_total_size_bytes());
    if (!asset_loader.load_prefab_payload(handle, payload))
    {
        MIZU_ASSERT(false, "Failed to load prefab: {}", handle.get_id());
        return invalid_controller;
    }

    const size_t element_count = record->metadata.get_total_size_bytes() / sizeof(PrefabMeshInfo);
    std::span<const PrefabMeshInfo> prefab_payload{
        reinterpret_cast<const PrefabMeshInfo*>(payload.data()), element_count};

    for (const PrefabMeshInfo& info : prefab_payload)
    {
        const StaticMeshStaticState mesh_ss{
            .transform_handle = entry.transform_handle,
            .mesh_handle = info.mesh_handle,
            .material_handle = info.material_handle,
        };

        const StaticMeshHandle mesh_handle = g_static_mesh_state_manager->sim_create(mesh_ss, StaticMeshDynamicState{});
        entry.static_meshes.push_back(mesh_handle);
    }

    auto inserted_it = m_prefab_entries_map.emplace(entry.id, entry);
    if (!inserted_it.second)
    {
        MIZU_ASSERT(false, "Failed to insert PrefabEntry");
        return invalid_controller;
    }

    return PrefabController{this, entry.id};
}

PrefabEntry* PrefabManager::find_entry(size_t id)
{
    const auto it = m_prefab_entries_map.find(id);
    return it != m_prefab_entries_map.end() ? &it->second : nullptr;
}

} // namespace Mizu
