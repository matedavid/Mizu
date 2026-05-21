#include "mesh_manager.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "resources/residency_manager.h"

namespace Mizu
{

static constexpr size_t PendingUpdatesBufferSize = StaticMeshConfig::MaxNumHandles * 2;

MeshManager::MeshManager(ResidencyManager& residency_manager)
    : m_residency_manager(residency_manager)
    , m_pending_updates(PendingUpdatesBufferSize)
{
    MIZU_ASSERT(
        g_static_mesh_state_manager != nullptr, "StaticMeshStateManager must be initialized before MeshManager");
    g_static_mesh_state_manager->register_rend_consumer(this);
}

MeshManager::~MeshManager()
{
    if (g_static_mesh_state_manager != nullptr)
        g_static_mesh_state_manager->unregister_rend_consumer(this);
}

void MeshManager::update()
{
    MIZU_PROFILE_SCOPED;

    size_t pending_updates_count = m_pending_updates.size();

    PendingUpdate update{};
    while (pending_updates_count > 0)
    {
        if (!m_pending_updates.pop(update))
        {
            MIZU_ASSERT(
                false,
                "Failed to pop pending update. This should never happen since we check the size before popping.");
            break;
        }

        switch (update.type)
        {
        case PendingUpdate::Type::Create:
            rend_apply_create_update(update);
            break;
        case PendingUpdate::Type::Destroy:
            rend_apply_destroy_update(update);
            break;
        case PendingUpdate::Type::Loading:
            rend_apply_loading_update(update);
            break;
        }

        pending_updates_count -= 1;
    }

    for (MeshManagerEntry& entry : m_meshes)
    {
        entry.transform_ds = g_transform_state_manager->rend_get_dynamic_state(entry.transform_handle);
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
    PendingUpdate update{};
    update.type = PendingUpdate::Type::Create;
    update.handle = handle;
    update.mesh_handle = ss.mesh_handle;
    update.ss = ss;

    m_pending_updates.push(update);
}

void MeshManager::rend_on_update(
    [[maybe_unused]] StaticMeshHandle handle,
    [[maybe_unused]] const StaticMeshDynamicState& ds)
{
    // Dynamic state is empty
}

void MeshManager::rend_on_destroy(StaticMeshHandle handle)
{
    PendingUpdate update{};
    update.type = PendingUpdate::Type::Destroy;
    update.handle = handle;
    update.mesh_handle = g_static_mesh_state_manager->get_static_state(handle).mesh_handle;

    m_pending_updates.push(update);
}

void MeshManager::rend_apply_create_update(const PendingUpdate& update)
{
    if (update.type != PendingUpdate::Type::Create)
    {
        MIZU_ASSERT(false, "Invalid update type for rend_apply_create_update");
        return;
    }

    const MeshAssetHandle mesh_handle = update.mesh_handle;
    if (!mesh_handle.is_valid())
    {
        MIZU_LOG_ERROR("Trying to load invalid MeshAssetHandle");
        return;
    }

    const ResidencyStatus status = m_residency_manager.request_load(mesh_handle);
    if (status == ResidencyStatus::Loading)
    {
        PendingUpdate loading_update = update;
        loading_update.type = PendingUpdate::Type::Loading;

        m_pending_updates.push(loading_update);
    }
    else if (status == ResidencyStatus::Loaded)
    {
        const std::optional<GpuMeshAllocationHandle> gpu_allocation = query_loaded_gpu_mesh_allocation(mesh_handle);
        if (!gpu_allocation.has_value())
        {
            MIZU_ASSERT(false, "Mesh residency is loaded but no GPU allocation is available");
            return;
        }

        add_mesh(update.handle, update.ss, *gpu_allocation);
    }
    else
    {
        MIZU_UNREACHABLE("Unexpected residency status for mesh asset handle: {}", mesh_handle.get_id());
    }
}

void MeshManager::rend_apply_destroy_update(const PendingUpdate& update)
{
    if (update.type != PendingUpdate::Type::Destroy)
    {
        MIZU_ASSERT(false, "Invalid update type for rend_apply_destroy_update");
        return;
    }

    const MeshAssetHandle mesh_handle = update.mesh_handle;
    if (!mesh_handle.is_valid())
    {
        MIZU_LOG_ERROR("Trying to load invalid MeshAssetHandle");
        return;
    }

    const bool unloading = m_residency_manager.request_unload(mesh_handle);
    if (!unloading)
    {
        MIZU_UNREACHABLE("Unexpected failure to request unload for mesh asset handle: {}", mesh_handle.get_id());
        return;
    }

    remove_mesh(update.handle);
}

void MeshManager::rend_apply_loading_update(const PendingUpdate& update)
{
    if (update.type != PendingUpdate::Type::Loading)
    {
        MIZU_ASSERT(false, "Invalid update type for rend_apply_loading_update");
        return;
    }

    const MeshAssetHandle mesh_handle = update.mesh_handle;
    if (!mesh_handle.is_valid())
    {
        MIZU_LOG_ERROR("Trying to load invalid MeshAssetHandle");
        return;
    }

    const ResidencyStatus status = m_residency_manager.get_status(mesh_handle);
    if (status == ResidencyStatus::Loaded)
    {
        const std::optional<GpuMeshAllocationHandle> gpu_allocation = query_loaded_gpu_mesh_allocation(mesh_handle);
        if (!gpu_allocation.has_value())
        {
            MIZU_ASSERT(false, "Mesh residency is loaded but no GPU allocation is available");
            return;
        }

        add_mesh(update.handle, update.ss, *gpu_allocation);
    }
    else if (status == ResidencyStatus::Loading)
    {
        m_pending_updates.push(update);
    }
    else
    {
        MIZU_ASSERT(
            false, "Unexpected residency status for mesh asset handle: " + std::to_string(static_cast<int>(status)));
    }
}

std::optional<GpuMeshAllocationHandle> MeshManager::query_loaded_gpu_mesh_allocation(MeshAssetHandle mesh_handle) const
{
    const ResidencyManager::MeshResidencySnapshot residency = m_residency_manager.query_mesh_residency(mesh_handle);
    if (residency.status != ResidencyStatus::Loaded)
        return std::nullopt;

    MIZU_ASSERT(residency.allocation.has_value(), "Loaded mesh is missing its GPU allocation handle");
    return residency.allocation;
}

void MeshManager::add_mesh(
    StaticMeshHandle handle,
    const StaticMeshStaticState& ss,
    const GpuMeshAllocationHandle& gpu_allocation)
{
    m_meshes.push_back(
        MeshManagerEntry{
            .handle = handle,
            .transform_handle = ss.transform_handle,
            .transform_ds = TransformDynamicState{},
            .gpu_mesh_handle = gpu_allocation,
            .mesh = ss.mesh,
            .material = ss.material,
        });
}

void MeshManager::remove_mesh(StaticMeshHandle handle)
{
    const auto new_end = std::remove_if(
        m_meshes.begin(), m_meshes.end(), [handle](const MeshManagerEntry& entry) { return entry.handle == handle; });
    m_meshes.erase(new_end, m_meshes.end());
}

MeshManager* s_mesh_manager = nullptr;

void mesh_manager_init(ResidencyManager& residency_manager)
{
    MIZU_ASSERT(s_mesh_manager == nullptr, "MeshManager is already initialized");
    s_mesh_manager = new MeshManager{residency_manager};
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
