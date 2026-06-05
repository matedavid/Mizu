#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"
#include "core/job_system/mpsc_queue.h"
#include "render_core/rhi/descriptors.h"

#include "resources/asset_load_system.h"
#include "resources/resource_event_stream.h"
#include "resources/streaming_planner.h"

namespace Mizu
{

class AssetLoadSystem;
class BufferResource;
class GpuTexturePool;
struct GpuMeshAllocationHandle;
struct GpuTextureAllocationHandle;

enum class ResidencyStatus2
{
    Unloaded,
    Loading,
    GpuResident,
};

template <typename AssetHandleType, typename RecordPayload>
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

        RecordPayload payload{};
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

struct MeshResidencySystemPayload
{
    std::optional<GpuMeshResidentRecord> resident_record;
};

class MeshResidencySystem : public ResidencySystemBase<MeshAssetHandle, MeshResidencySystemPayload>
{
  public:
    MeshResidencySystem(AssetLoadSystem& load_system, StreamingMeshRequestQueue& request_queue);

    void update(ResourceEventStream& stream);

    std::optional<GpuMeshResidentRecord> get_gpu_resident_record(const MeshAssetHandle& handle) const;

  private:
    AssetLoadSystem& m_load_system;
    StreamingMeshRequestQueue& m_request_queue;
    MpscQueue<MeshResidencyEvent, MAX_STREAMING_REQUESTS> m_pending_events;

    void consume_requests();
    void track_evictions();
    void flush_pending_events(ResourceEventStream& stream);

    void request_load(const MeshStreamingRequest& request);
    void request_eviction(const MeshStreamingRequest& request);

    void cpu_load_finished(const MeshAssetHandle& handle, const CpuAllocationHandle& allocation_handle);
    void gpu_load_finished(const MeshAssetHandle& handle, const GpuMeshResidentRecord& resident_record);
};

struct TextureResidencySystemPayload
{
    std::optional<GpuTextureResidentRecord> resident_record;
    uint32_t bindless_descriptor_slot = std::numeric_limits<uint32_t>::max();
};

class TextureResidencySystem : public ResidencySystemBase<TextureAssetHandle, TextureResidencySystemPayload>
{
  public:
    TextureResidencySystem(
        AssetLoadSystem& load_system,
        StreamingTextureRequestQueue& request_queue,
        GpuTexturePool& gpu_texture_pool);

    void update(ResourceEventStream& stream);

    void request_dependency_load(const TextureAssetHandle& handle);
    void request_dependency_evict(const TextureAssetHandle& handle);

    std::optional<uint32_t> get_bindless_descriptor_slot(const TextureAssetHandle& handle) const;

    std::shared_ptr<DescriptorSet> get_bindless_descriptor_set() const { return m_bindless_texture_descriptor_set; }

  private:
    AssetLoadSystem& m_load_system;
    StreamingTextureRequestQueue& m_request_queue;
    GpuTexturePool& m_gpu_texture_pool;
    MpscQueue<TextureResidencyEvent, MAX_STREAMING_REQUESTS> m_pending_events;

    std::shared_ptr<DescriptorSet> m_bindless_texture_descriptor_set;
    std::vector<uint32_t> m_free_bindless_slots;

    void consume_requests();
    void track_evictions();
    void flush_pending_events(ResourceEventStream& stream);

    void request_load(const TextureStreamingRequest& request);
    void request_eviction(const TextureStreamingRequest& request);

    void cpu_load_finished(const TextureAssetHandle& handle, const CpuAllocationHandle& allocation_handle);
    void gpu_load_finished(const TextureAssetHandle& handle, const GpuTextureResidentRecord& resident_record);

    std::optional<uint32_t> allocate_bindless_descriptor_slot();
    void free_bindless_descriptor_slot(uint32_t slot);
};

struct MaterialResidencySystemPayload
{
    uint32_t material_buffer_slot = std::numeric_limits<uint32_t>::max();
};

class MaterialResidencySystem : public ResidencySystemBase<MaterialAssetHandle, MaterialResidencySystemPayload>
{
  public:
    MaterialResidencySystem(
        AssetLoadSystem& load_system,
        StreamingMaterialRequestQueue& request_queue,
        TextureResidencySystem& texture_residency_system);

    void update(ResourceEventStream& stream);

    std::optional<uint32_t> get_material_buffer_slot(const MaterialAssetHandle& handle) const;

  private:
    AssetLoadSystem& m_load_system;
    StreamingMaterialRequestQueue& m_request_queue;
    TextureResidencySystem& m_texture_residency_system;
    MpscQueue<MaterialResidencyEvent, MAX_STREAMING_REQUESTS> m_pending_events;

    std::vector<MaterialAssetRecord> m_pending_records;

    static constexpr uint64_t MAX_TEXTURES_PER_MATERIAL = 16;

    std::shared_ptr<BufferResource> m_material_buffer;
    std::vector<uint32_t> m_free_material_buffer_slots;

    void consume_requests();
    void refresh_pending_materials();
    void flush_pending_events(ResourceEventStream& stream);

    void request_load(const MaterialStreamingRequest& request);
    void request_eviction(const MaterialStreamingRequest& request);

    bool material_dependencies_loaded(const MaterialAssetRecord& record) const;
    void material_load_finished(const MaterialAssetRecord& record);

    std::optional<uint32_t> allocate_material_buffer_slot();
    void free_material_buffer_slot(uint32_t slot);
};

} // namespace Mizu
