#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"
#include "asset/asset_metadata.h"
#include "core/job_system/mpsc_queue.h"

#include "render/resources/gpu_resource_types.h"
#include "render/state_manager/transform_state_manager.h"
#include "resources/residency_system.h"
#include "streaming/staging_pool.h"
#include "streaming/streaming_manager.h"

namespace Mizu
{

class AssetLoadScheduler;
class GpuMeshPool;
class IAssetLoader;

struct MeshStreamingHandle
{
    uint64_t id;

    MeshStreamingHandle() = default;
    MeshStreamingHandle(uint64_t id_) : id(id_) {}

    bool operator==(const MeshStreamingHandle& other) const { return id == other.id; }
};

struct MeshStreamingUpdate
{
    MeshAssetHandle handle{};
    GpuMeshDrawPayload draw_payload{};
    bool resident = false;
};

struct MeshStreamingUsage
{
    MeshStreamingHandle streaming_handle;

    MeshAssetHandle mesh_handle;
    TransformHandle transform_handle;
};

class MeshStreamingManager : public IStreamingManager
{
  public:
    MeshStreamingManager(
        IAssetLoader& asset_loader,
        AssetLoadScheduler& load_scheduler,
        StagingPool& staging_pool,
        GpuMeshPool& gpu_mesh_pool);
    ~MeshStreamingManager() override = default;

    void update(const StreamingContext& context) override;
    void gather(std::vector<StreamingRequest>& requests) override;
    void execute(std::span<StreamingAward> awards) override;

    void record_gpu_uploads(CommandBuffer& command, FrameLinearAllocator& frame_allocator) override;

    void add_usage(const MeshStreamingUsage& usage);
    void remove_usage(MeshStreamingHandle streaming_handle);

    bool is_drawable(MeshAssetHandle handle) const;
    std::span<const MeshStreamingUpdate> get_updates() const { return m_mesh_updates; }

  private:
    IAssetLoader& m_asset_loader;
    AssetLoadScheduler& m_load_scheduler;
    StagingPool& m_staging_pool;
    GpuMeshPool& m_gpu_mesh_pool;

    struct MeshResidencyPayload
    {
        uint8_t resident_level = 0;
        uint8_t desired_level = 0;

        GpuMeshAllocationHandle allocation{};
    };

    ResidencySystemBase<MeshAssetHandle, MeshResidencyPayload> m_residency{};

    struct MeshStreamingUsageHasher
    {
        size_t operator()(const MeshStreamingHandle& handle) const { return handle.id; }
    };

    std::vector<MeshAssetHandle> m_streamable_meshes{};

    std::unordered_map<MeshStreamingHandle, MeshStreamingUsage, MeshStreamingUsageHasher> m_usages_map{};
    std::unordered_map<MeshAssetHandle, std::vector<MeshStreamingUsage>> m_mesh_to_usages_map{};

    std::vector<StreamingRequest> m_pending_requests{};
    std::vector<MeshAssetHandle> m_dirty_handles{};
    std::vector<MeshStreamingUpdate> m_mesh_updates{};

    uint32_t m_update_cursor = 0;
    uint32_t m_iterations_per_update = 0;

    struct GpuUploadRecord
    {
        MeshAssetHandle handle{};
        MeshAssetMetadata metadata{};
        StreamingRequest request{};

        GpuMeshAllocationHandle allocation{};
        StagingBlock staging_block{};
    };

    MpscQueue<GpuUploadRecord, 512> m_gpu_upload_queue{};

    struct EvaluateResult
    {
        uint8_t desired_level;
        float score;
    };

    void refresh(MeshAssetHandle handle, const StreamingContext& context);
    std::optional<EvaluateResult> evaluate(
        MeshAssetHandle handle,
        const StreamingContext& context,
        const MeshResidencyPayload& payload,
        uint32_t references);

    bool has_pending_request_for_handle(MeshAssetHandle handle) const;
    bool is_streamable(MeshAssetHandle handle) const;
};

} // namespace Mizu
