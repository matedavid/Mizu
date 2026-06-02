#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "resources/asset_load_system.h"
#include "resources/streaming_planner.h"

namespace Mizu
{

class AssetLoadSystem;

enum class ResidencyStatus2
{
    Unloaded,
    Loading,
    GpuResident,
};

template <typename AssetHandleType>
class ResidencySystemBase
{
    static_assert(IsAssetHandleType<AssetHandleType>, "AssetHandleType must be a valid AssetHandle type");

  public:
    virtual ~ResidencySystemBase() = default;

    virtual ResidencyStatus2 get_status(const AssetHandleType& handle) const;

  protected:
    struct Record
    {
        AssetHandleType handle{};
        std::atomic<ResidencyStatus2> status{ResidencyStatus2::Unloaded};
        std::atomic<size_t> references{0};
    };

    static constexpr size_t NUM_SHARDS = 16;

    struct Shard
    {
        std::unordered_map<size_t, Record> records;
        mutable std::mutex mutex;
    };

    std::array<Shard, NUM_SHARDS> m_shards{};

    bool increment_reference_count(const AssetHandleType& handle);
    bool decrement_reference_count(const AssetHandleType& handle);

    bool transition_status(const AssetHandleType& handle, ResidencyStatus2 expected, ResidencyStatus2 desired);

    Record* get_record(const AssetHandleType& handle);
    const Record* get_record(const AssetHandleType& handle) const;

    Record* get_or_create_record(const AssetHandleType& handle);

    Shard& get_shard(const AssetHandleType& handle);
    const Shard& get_shard(const AssetHandleType& handle) const;
};

class MeshResidencySystem : public ResidencySystemBase<MeshAssetHandle>
{
  public:
    MeshResidencySystem(AssetLoadSystem& load_system, StreamingMeshRequestQueue& request_queue);

    void update();

  private:
    AssetLoadSystem& m_load_system;
    StreamingMeshRequestQueue& m_request_queue;

    void consume_requests();
    void track_evictions();

    void request_load(const MeshStreamingRequest& request);
    void request_eviction(const MeshStreamingRequest& request);

    void cpu_load_finished(const MeshAssetHandle& handle, const CpuAllocationHandle& allocation_handle);
    void gpu_load_finished(const MeshAssetHandle& handle, const GpuMeshAllocationHandle& allocation_handle);
};

class TextureResidencySystem : public ResidencySystemBase<TextureAssetHandle>
{
  public:
    TextureResidencySystem(AssetLoadSystem& load_system, StreamingTextureRequestQueue& request_queue);

    void update();

    void request_dependency_load(const TextureAssetHandle& handle);
    void request_dependency_evict(const TextureAssetHandle& handle);

  private:
    AssetLoadSystem& m_load_system;
    StreamingTextureRequestQueue& m_request_queue;

    void consume_requests();
    void track_evictions();

    void request_load(const TextureStreamingRequest& request);
    void request_eviction(const TextureStreamingRequest& request);

    void cpu_load_finished(const TextureAssetHandle& handle, const CpuAllocationHandle& allocation_handle);
    void gpu_load_finished(const TextureAssetHandle& handle, const GpuTextureAllocationHandle& allocation_handle);
};

class MaterialResidencySystem : public ResidencySystemBase<MaterialAssetHandle>
{
  public:
    MaterialResidencySystem(
        AssetLoadSystem& load_system,
        StreamingMaterialRequestQueue& request_queue,
        TextureResidencySystem& texture_residency_system);

    void update();

  private:
    AssetLoadSystem& m_load_system;
    StreamingMaterialRequestQueue& m_request_queue;
    TextureResidencySystem& m_texture_residency_system;

    std::vector<MaterialAssetRecord> m_pending_records;

    void consume_requests();
    void refresh_pending_materials();

    void request_load(const MaterialStreamingRequest& request);
    void request_eviction(const MaterialStreamingRequest& request);

    bool material_dependencies_loaded(const MaterialAssetRecord& record) const;

    void material_load_finished(const MaterialAssetRecord& record);
};

} // namespace Mizu
