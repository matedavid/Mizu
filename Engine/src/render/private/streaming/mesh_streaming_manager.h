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
#include "streaming/mesh_streaming_pool.h"
#include "streaming/staging_pool.h"
#include "streaming/streaming_manager.h"

namespace Mizu
{

class AssetLoadScheduler;
class GpuPageAllocator;
class IAssetLoader;
class SparseBindScheduler;

struct MeshStreamingHandle
{
    uint64_t id;

    MeshStreamingHandle() = default;
    MeshStreamingHandle(uint64_t id_) : id(id_) {}

    bool operator==(const MeshStreamingHandle& other) const { return id == other.id; }
};

struct MeshStreamingResidency
{
    GpuMeshAllocationHandle allocation_handle{};
    MeshAssetMetadata metadata{};
    uint8_t resident_first_lod = 0;
};

struct MeshStreamingUpdate
{
    MeshAssetHandle handle{};
    MeshStreamingResidency residency{};
    bool resident = false;
};

struct MeshStreamingUsage
{
    MeshStreamingHandle streaming_handle{};

    MeshAssetHandle mesh_handle{};
    TransformHandle transform_handle{};
};

struct MeshStreamingManagerDescription
{
    MeshStreamingPoolDescription pool{};
};

class MeshStreamingManager : public IStreamingManager
{
  public:
    MeshStreamingManager(
        MeshStreamingManagerDescription desc,
        IAssetLoader& asset_loader,
        AssetLoadScheduler& load_scheduler,
        StagingPool& staging_pool,
        GpuPageAllocator& page_allocator,
        SparseBindScheduler& sparse_bind_scheduler);
    ~MeshStreamingManager() override = default;

    void update(const StreamingContext& context) override;
    void gather(std::vector<StreamingRequest>& requests) override;
    void execute(std::span<StreamingAward> awards) override;
    void post_execute() override;

    void setup_render_graph_upload_pass(RenderGraphBuilder& builder, RenderGraphPassBuilder& pass) override;
    void record_gpu_uploads(CommandBuffer& command, FrameLinearAllocator& frame_allocator) override;

    void add_usage(const MeshStreamingUsage& usage);
    void remove_usage(MeshStreamingHandle streaming_handle);

    std::optional<MeshStreamingResidency> get_mesh_residency(MeshAssetHandle handle) const;

    bool is_drawable(MeshAssetHandle handle) const;
    std::span<const MeshStreamingUpdate> get_updates() const { return m_mesh_updates; }

    const MeshStreamingPool& get_streaming_pool() const { return m_mesh_streaming_pool; }

  private:
    IAssetLoader& m_asset_loader;
    AssetLoadScheduler& m_load_scheduler;
    StagingPool& m_staging_pool;
    MeshStreamingPool m_mesh_streaming_pool;

    struct MeshResidencyPayload
    {
        uint8_t resident_level = 0;
        uint8_t desired_level = 0;

        MeshStreamingResidency residency{};
    };

    ResidencySystemBase<MeshAssetHandle, MeshResidencyPayload> m_residency{};

    struct MeshStreamingUsageHasher
    {
        size_t operator()(const MeshStreamingHandle& handle) const { return handle.id; }
    };

    struct MeshEntry
    {
        MeshAssetMetadata metadata{}; // Cached once, when the mesh is first registered
        std::vector<MeshStreamingUsage> usages{};

        bool is_streamable() const { return metadata.streamable_lods > 0 && metadata.get_num_lods() > 1; }
    };

    std::vector<MeshAssetHandle> m_streamable_meshes{};

    std::unordered_map<MeshAssetHandle, MeshEntry> m_meshes{};
    std::unordered_map<MeshStreamingHandle, MeshAssetHandle, MeshStreamingUsageHasher> m_usage_to_mesh{};

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

        StagingBlock staging_block{};
    };

    MpscQueue<GpuUploadRecord, 512> m_gpu_upload_queue{};

    struct EvaluateResult
    {
        uint8_t desired_level;
        float score;
        uint64_t staging_size_bytes;
    };

    void refresh(MeshAssetHandle handle, const StreamingContext& context);
    std::optional<EvaluateResult> evaluate(
        const MeshEntry& entry,
        const StreamingContext& context,
        const MeshResidencyPayload& payload,
        uint32_t references);

    MeshEntry* find_entry(MeshAssetHandle handle);
    MeshEntry* register_mesh(MeshAssetHandle handle);
    void unregister_mesh(MeshAssetHandle handle);

    bool has_pending_request_for_handle(MeshAssetHandle handle) const;
};

} // namespace Mizu