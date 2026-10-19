#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"
#include "base/math/aabb.h"
#include "core/job_system/mpsc_queue.h"

#include "render/state_manager/transform_state_manager.h"
#include "resources/residency_system.h"
#include "streaming/staging_pool.h"
#include "streaming/streaming_manager.h"

namespace Mizu
{

class AssetLoadScheduler;
class GpuTexturePool;
class IAssetLoader;

class TexturePool
{
  public:
    ImageResource* allocate(const ImageDescription& desc);
    void free(ImageResource* resource, uint32_t eviction_frames);

    void track_evictions();

  private:
    std::unordered_map<uint64_t, std::shared_ptr<ImageResource>> m_textures_map{};

    struct Eviction
    {
        uint64_t id;
        uint32_t frames;
    };
    std::vector<Eviction> m_evictions{};
};

struct TextureStreamingHandle
{
    uint64_t id;

    TextureStreamingHandle() = default;
    TextureStreamingHandle(uint64_t id_) : id(id_) {}

    bool operator==(const TextureStreamingHandle& other) const { return id == other.id; }
};

struct TextureStreamingUsage
{
    TextureStreamingHandle streaming_handle;

    TextureAssetHandle texture_handle;
    TransformHandle transform_handle;
    math::AABB aabb;
    float texel_factor;
};

class TextureStreamingManager : public IStreamingManager
{
  public:
    TextureStreamingManager(IAssetLoader& asset_loader, AssetLoadScheduler& load_scheduler, StagingPool& staging_pool);
    ~TextureStreamingManager() override = default;

    void update(const StreamingContext& context) override;
    void gather(std::vector<StreamingRequest>& requests) override;
    void execute(std::span<StreamingAward> awards) override;

    void record_gpu_uploads(CommandBuffer& command, FrameLinearAllocator& frame_allocator) override;

    void add_usage(const TextureStreamingUsage& usage);
    void remove_usage(TextureStreamingHandle streaming_handle);

  private:
    IAssetLoader& m_asset_loader;
    AssetLoadScheduler& m_load_scheduler;
    StagingPool& m_staging_pool;

    struct TextureResidencyPayload
    {
        uint8_t resident_level = 0;
        uint8_t desired_level = 0;

        ImageResource* resource; // Owned by the TexturePool
    };

    ResidencySystemBase<TextureAssetHandle, TextureResidencyPayload> m_residency{};

    struct TextureStreamingUsageHasher
    {
        size_t operator()(const TextureStreamingHandle& handle) const { return handle.id; }
    };

    std::unordered_map<TextureStreamingHandle, TextureStreamingUsage, TextureStreamingUsageHasher> m_usages_map{};
    std::vector<TextureAssetHandle> m_streamable_textures{};

    std::unordered_map<TextureAssetHandle, std::vector<TextureStreamingUsage>> m_texture_to_usages_map{};

    std::vector<StreamingRequest> m_pending_requests{};
    std::vector<TextureAssetHandle> m_dirty_handles{};

    uint32_t m_update_cursor = 0;
    uint32_t m_iterations_per_update = 0;

    TexturePool m_texture_pool{};

    struct GpuUploadRecord
    {
        TextureAssetHandle handle{};
        TextureAssetMetadata metadata{};
        StreamingRequest request{};

        ImageResource* new_texture = nullptr;
        ImageResource* old_texture = nullptr;

        StagingBlock staging_block{};
    };

    MpscQueue<GpuUploadRecord, 512> m_gpu_upload_queue{};

    struct EvaluateResult
    {
        uint8_t desired_level;
        float score;
    };

    void refresh(TextureAssetHandle handle, const StreamingContext& context);
    std::optional<EvaluateResult> evaluate(
        TextureAssetHandle handle,
        const StreamingContext& context,
        const TextureResidencyPayload& payload,
        uint32_t references);

    void finalize_texture_upload(TextureAssetHandle handle, uint8_t resident_level);

    bool is_streamable(TextureAssetHandle handle) const;
};

} // namespace Mizu
