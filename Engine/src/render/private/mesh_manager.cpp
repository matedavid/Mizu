#include "mesh_manager.h"

namespace Mizu
{

MeshManager::MeshManager()
{
    MIZU_ASSERT(
        g_static_mesh_state_manager2 != nullptr, "StaticMeshStateManager2 must be initialized before MeshManager");
    g_static_mesh_state_manager2->register_rend_consumer(this);
}

MeshManager::~MeshManager()
{
    if (g_static_mesh_state_manager2 != nullptr)
        g_static_mesh_state_manager2->unregister_rend_consumer(this);
}

void MeshManager::update()
{
    for (MeshManagerEntry& entry : m_meshes)
    {
        entry.transform_ds = g_transform_state_manager2->rend_get_dynamic_state(entry.transform_handle);
    }
}

std::span<const MeshManagerEntry> MeshManager::get_meshes() const
{
    return m_meshes;
}

void MeshManager::rend_on_create(
    StaticMeshHandle handle,
    const StaticMeshStaticState& ss,
    [[maybe_unused]] const StaticMeshDynamicState& ds)
{
    MeshManagerEntry entry{};
    entry.handle = handle;
    entry.transform_handle = ss.transform_handle;
    entry.transform_ds = TransformDynamicState{};
    entry.mesh = ss.mesh;
    entry.material = ss.material;

    m_meshes.push_back(entry);
}

void MeshManager::rend_on_update(
    [[maybe_unused]] StaticMeshHandle handle,
    [[maybe_unused]] const StaticMeshDynamicState& ds)
{
    // Dynamic state is empty
}

void MeshManager::rend_on_destroy(StaticMeshHandle handle)
{
    const auto new_end = std::remove_if(
        m_meshes.begin(), m_meshes.end(), [handle](const MeshManagerEntry& entry) { return entry.handle == handle; });
    m_meshes.erase(new_end, m_meshes.end());
}

MeshManager* s_mesh_manager = nullptr;

void mesh_manager_init()
{
    MIZU_ASSERT(s_mesh_manager == nullptr, "MeshManager is already initialized");
    s_mesh_manager = new MeshManager{};
}

void mesh_manager_shutdown()
{
    delete s_mesh_manager;
    s_mesh_manager = nullptr;
}

void mesh_manager_update()
{
    MeshManager& mesh_manager = const_cast<MeshManager&>(mesh_manager_get());
    mesh_manager.update();
}

const MeshManager& mesh_manager_get()
{
    MIZU_ASSERT(s_mesh_manager != nullptr, "MeshManager is not initialized");
    return *s_mesh_manager;
}

} // namespace Mizu
