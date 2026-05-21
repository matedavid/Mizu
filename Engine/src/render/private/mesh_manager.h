#pragma once

#include <span>
#include <vector>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"

#include "render/material/material.h"
#include "render/model/mesh.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "resources/gpu_pools.h"

namespace Mizu
{

class ResidencyManager;

struct MeshManagerEntry
{
    StaticMeshHandle handle{};
    TransformHandle transform_handle{};

    TransformDynamicState transform_ds{};
    GpuMeshAllocationHandle gpu_mesh_handle{};

    std::shared_ptr<Mesh> mesh{};
    std::shared_ptr<Material> material{};
};

class MeshManager : public StaticMeshStateManagerConsumer
{
  public:
    MeshManager(ResidencyManager& residency_manager);
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
    ResidencyManager& m_residency_manager;

    inplace_vector<MeshManagerEntry, StaticMeshConfig::MaxNumHandles> m_meshes{};

    struct PendingUpdate
    {
        enum class Type
        {
            Create,
            Destroy,
            Loading,
        };

        Type type;

        StaticMeshHandle handle;
        MeshAssetHandle mesh_handle;
        StaticMeshStaticState ss;
    };

    class PendingUpdatesCircularBuffer
    {
      public:
        explicit PendingUpdatesCircularBuffer(size_t size) : m_updates(size)
        {
            MIZU_ASSERT(size > 0, "Circular buffer size must be greater than 0.");
        }

        bool push(const PendingUpdate& update)
        {
            if (m_size == m_updates.size())
            {
                MIZU_ASSERT(false, "Pending updates buffer overflow. Consider increasing the buffer size.");
                return false;
            }

            m_updates[m_head] = update;
            m_head = (m_head + 1) % m_updates.size();
            m_size += 1;

            return true;
        }

        bool pop(PendingUpdate& update)
        {
            if (m_size == 0)
            {
                MIZU_ASSERT(false, "Trying to pop from an empty pending updates buffer.");
                return false;
            }

            update = m_updates[m_tail];
            m_tail = (m_tail + 1) % m_updates.size();
            m_size -= 1;

            return true;
        }

        bool empty() const { return m_size == 0; }
        size_t size() const { return m_size; }

      private:
        std::vector<PendingUpdate> m_updates;

        size_t m_head{0};
        size_t m_tail{0};
        size_t m_size{0};
    };

    PendingUpdatesCircularBuffer m_pending_updates;

    void rend_apply_create_update(const PendingUpdate& update);
    void rend_apply_destroy_update(const PendingUpdate& update);
    void rend_apply_loading_update(const PendingUpdate& update);

    std::optional<GpuMeshAllocationHandle> query_loaded_gpu_mesh_allocation(MeshAssetHandle mesh_handle) const;

    void add_mesh(
        StaticMeshHandle handle,
        const StaticMeshStaticState& ss,
        const GpuMeshAllocationHandle& gpu_allocation);
    void remove_mesh(StaticMeshHandle handle);
};

void mesh_manager_init(ResidencyManager& residency_manager);
void mesh_manager_shutdown();
void mesh_manager_update();
const MeshManager& mesh_manager_get();

} // namespace Mizu
