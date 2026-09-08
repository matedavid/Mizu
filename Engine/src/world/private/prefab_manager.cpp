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
    if (!is_valid())
    {
        MIZU_ASSERT(false, "Invalid PrefabController");
        return;
    }

    g_transform_state_manager->sim_update(m_entry->transform_handle, ds);
}

TransformDynamicState PrefabController::get_transform_ds() const
{
    if (!is_valid())
    {
        MIZU_ASSERT(false, "Invalid PrefabController");
        return TransformDynamicState{};
    }

    return g_transform_state_manager->sim_get_dynamic_state(m_entry->transform_handle);
}

TransformDynamicState& PrefabController::edit_transform_ds()
{
    MIZU_ASSERT(is_valid(), "Invalid PrefabController");
    return g_transform_state_manager->sim_edit(m_entry->transform_handle);
}

bool PrefabController::is_valid() const
{
    return m_owner != nullptr && m_entry != nullptr && m_owner->is_entry_valid(m_entry);
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
    MIZU_ASSERT(controller.is_valid(), "Trying to destroy invalid PrefabController");

    auto it = m_prefab_entries_map.find(controller.m_entry->id);
    if (it == m_prefab_entries_map.end())
    {
        MIZU_LOG_ERROR("Trying to destroy PrefabController that does not exist");
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
    PrefabController invalid_controller{this, nullptr};

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

    return PrefabController{this, &inserted_it.first->second};
}

bool PrefabManager::is_entry_valid(PrefabEntry* entry) const
{
    if (entry == nullptr)
        return false;

    return m_prefab_entries_map.contains(entry->id);
}

} // namespace Mizu
