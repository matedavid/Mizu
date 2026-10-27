#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"
#include "asset/asset_metadata.h"
#include "base/math/shapes.h"
#include "core/job_system/mpsc_queue.h"

#include "render/state_manager/transform_state_manager.h"
#include "resources/residency_system.h"
#include "streaming/staging_pool.h"
#include "streaming/streaming_manager.h"

namespace Mizu
{

class AssetLoadScheduler;
class IAssetLoader;

struct TextureStreamingHandle
{
    uint64_t id;

    TextureStreamingHandle() = default;
    TextureStreamingHandle(uint64_t id_) : id(id_) {}

    bool operator==(const TextureStreamingHandle& other) const { return id == other.id; }
};

struct TextureStreamingUpdate
{
    TextureAssetHandle handle;
    ImageResource* new_texture;
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

    bool is_drawable(TextureAssetHandle handle) const;
    std::span<const TextureStreamingUpdate> get_updates() const { return m_texture_updates; }

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

    struct TextureEntry
    {
        TextureAssetMetadata metadata{}; // Cached once, when the texture is first registered
        std::vector<TextureStreamingUsage> usages{};

        bool is_streamable() const { return metadata.streamable_mips > 0 && metadata.num_mips > 1; }
    };

    std::vector<TextureAssetHandle> m_streamable_textures{};

    std::unordered_map<TextureAssetHandle, TextureEntry> m_textures{};
    std::unordered_map<TextureStreamingHandle, TextureAssetHandle, TextureStreamingUsageHasher> m_usage_to_texture{};

    std::vector<StreamingRequest> m_pending_requests{};
    std::vector<TextureAssetHandle> m_dirty_handles{};
    std::vector<TextureStreamingUpdate> m_texture_updates{};

    uint32_t m_update_cursor = 0;
    uint32_t m_iterations_per_update = 0;

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
        const TextureEntry& entry,
        const StreamingContext& context,
        const TextureResidencyPayload& payload,
        uint32_t references);

    TextureEntry* find_entry(TextureAssetHandle handle);
    TextureEntry* register_texture(TextureAssetHandle handle);

    bool has_pending_request_for_handle(TextureAssetHandle handle) const;
};

} // namespace Mizu