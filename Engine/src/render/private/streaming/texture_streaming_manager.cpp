#include "streaming/texture_streaming_manager.h"

#include <cstdint>
#include <cstring>
#include <format>

#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "base/math/geometric.h"
#include "base/math/math.h"
#include "base/math/shapes.h"
#include "render_core/rhi/image_resource.h"

#include "registries/render_view_registry.h"
#include "registries/transform_registry.h"
#include "resources/asset_load_scheduler.h"
#include "streaming/streaming_utils.h"

namespace Mizu
{

static constexpr uint32_t FRAMES_TO_UPDATE = 3u;
static constexpr uint32_t EVICTION_FRAMES = 2u;

TextureStreamingManager::TextureStreamingManager(
    IAssetLoader& asset_loader,
    AssetLoadScheduler& load_scheduler,
    StagingPool& staging_pool)
    : m_asset_loader(asset_loader)
    , m_load_scheduler(load_scheduler)
    , m_staging_pool(staging_pool)
{
}

void TextureStreamingManager::update(const StreamingContext& context)
{
    MIZU_PROFILE_SCOPED;

    m_texture_pool.track_evictions();
    m_texture_updates.clear();

    if (m_update_cursor >= FRAMES_TO_UPDATE)
    {
        m_update_cursor = 0;
    }

    if (m_update_cursor == 0)
    {
        m_iterations_per_update = math::ceil_div(static_cast<uint32_t>(m_streamable_textures.size()), FRAMES_TO_UPDATE);
    }

    for (TextureAssetHandle handle : m_dirty_handles)
    {
        refresh(handle, context);
    }

    m_dirty_handles.clear();

    for (uint32_t i = 0; i < m_iterations_per_update; ++i)
    {
        const uint32_t streamable_idx = m_update_cursor * m_iterations_per_update + i;
        if (streamable_idx >= m_streamable_textures.size())
            break;

        const TextureAssetHandle handle = m_streamable_textures[streamable_idx];
        refresh(handle, context);
    }

    m_update_cursor += 1;
}

void TextureStreamingManager::gather(std::vector<StreamingRequest>& requests)
{
    for (const StreamingRequest& pending : m_pending_requests)
    {
        requests.push_back(pending);
    }

    m_pending_requests.clear();
}

void TextureStreamingManager::execute(std::span<StreamingAward> awards)
{
    for (const StreamingAward& award : awards)
    {
        const StreamingRequest& request = award.request;

        const TextureAssetHandle handle = to_asset_handle<TextureAssetHandle>(request.asset_key);
        // TODO: Please cache this value please :(
        const TextureAssetMetadata metadata = m_asset_loader.get_texture_record(handle)->metadata;

        TextureResidencyPayload* payload = m_residency.get_payload(handle);
        if (payload == nullptr)
        {
            MIZU_UNREACHABLE("Failed to get payload");
            continue;
        }

        if (request.desired_level == 0)
        {
            // Eviction case, remove the texture

            if (!m_residency.transition_status(handle, ResidencyStatus::GpuResident, ResidencyStatus::Unloaded))
            {
                MIZU_UNREACHABLE("Failed to transition resource when evicting");
                continue;
            }

            m_texture_pool.free(payload->resource, EVICTION_FRAMES);

            m_texture_updates.push_back({
                .handle = handle,
                .new_texture = nullptr,
            });

            continue;
        }

        const uint32_t first_mip = metadata.num_mips - request.desired_level;
        const int32_t mip_delta =
            static_cast<int32_t>(request.desired_level) - static_cast<int32_t>(request.resident_level);

        MIZU_LOG_INFO(
            "Streaming handle={}, first_mip={}, mip_delta={}, desired_level={}, resident_level={}",
            request.asset_key.id,
            first_mip,
            mip_delta,
            request.desired_level,
            request.resident_level);

        StagingBlock block;

        if (mip_delta > 0)
        {
            const uint64_t size = metadata.get_mip_range_size_bytes(first_mip, static_cast<uint32_t>(mip_delta));

            const std::optional<StagingBlock> block_opt = m_staging_pool.allocate(size);
            if (!block_opt.has_value())
            {
                MIZU_ASSERT(false, "Failed to allocate from StagingPool");
                continue;
            }

            block = block_opt.value();
        }

        const glm::uvec2 mip_dims = compute_mip_size(metadata.width, metadata.height, first_mip);

        ImageDescription desc{};
        desc.width = mip_dims.x;
        desc.height = mip_dims.y;
        desc.depth = metadata.depth;
        desc.type = ImageType::Image2D;
        desc.format = metadata.format;
        desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst | ImageUsageBits::TransferSrc;
        desc.num_mips = metadata.num_mips - first_mip;
        desc.name = std::format("TextureStreamingManagerTexture_{}", handle.get_id());

        ImageResource* old_texture = payload->resource;

        ImageResource* new_texture = m_texture_pool.allocate(desc);
        payload->resource = new_texture;

        const ResidencyStatus expected_status =
            payload->resident_level == 0 ? ResidencyStatus::Unloaded : ResidencyStatus::GpuResident;
        if (!m_residency.transition_status(handle, expected_status, ResidencyStatus::Loading))
        {
            MIZU_UNREACHABLE("Failed to transition texture when requesting a load");
            continue;
        }

        if (mip_delta < 0)
        {
            // Streaming out case, no need to request texture load, just add the gpu upload

            m_gpu_upload_queue.push({
                .handle = handle,
                .metadata = metadata,
                .request = request,
                .new_texture = new_texture,
                .old_texture = old_texture,
                .staging_block = block,
            });

            continue;
        }

        m_load_scheduler.request_texture_load(
            handle,
            first_mip,
            static_cast<uint32_t>(mip_delta),
            block.as_span(),
            [this, metadata, request, new_texture, old_texture, block](TextureAssetHandle handle) {
                m_gpu_upload_queue.push({
                    .handle = handle,
                    .metadata = metadata,
                    .request = request,
                    .new_texture = new_texture,
                    .old_texture = old_texture,
                    .staging_block = block,
                });
            });
    }
}

struct MipUploadLayout
{
    glm::uvec2 dims;
    uint32_t row_pitch;
    uint64_t src_offset;
    uint64_t dst_offset;
};

static std::vector<MipUploadLayout> compute_mips_upload_layout(
    const TextureAssetMetadata& metadata,
    uint32_t first_mip,
    uint32_t mip_count,
    uint64_t row_pitch_alignment,
    uint64_t placement_alignment,
    uint64_t& out_total_size)
{
    const uint32_t format_size = get_image_format_size(metadata.format);
    const uint64_t src_base = metadata.get_mip_offset(first_mip);

    std::vector<MipUploadLayout> layouts;
    layouts.reserve(mip_count);

    uint64_t total = 0;
    for (uint32_t i = 0; i < mip_count; ++i)
    {
        const uint32_t mip = first_mip + i;
        const glm::uvec2 dims = compute_mip_size(metadata.width, metadata.height, mip);

        const uint32_t row_pitch = static_cast<uint32_t>(
            math::align_up_pow2(static_cast<uint64_t>(dims.x) * format_size, row_pitch_alignment));
        const uint64_t dst_offset = math::align_up_pow2(total, placement_alignment);

        layouts.push_back({dims, row_pitch, metadata.get_mip_offset(mip) - src_base, dst_offset});
        total = dst_offset + static_cast<uint64_t>(row_pitch) * dims.y * metadata.depth;
    }

    out_total_size = total;

    return layouts;
}

void TextureStreamingManager::record_gpu_uploads(CommandBuffer& command, FrameLinearAllocator& frame_allocator)
{
    const DeviceProperties& properties = g_render_device->get_properties();
    const uint64_t row_pitch_alignment = properties.min_texture_row_pitch_alignment;
    const uint64_t placement_alignment = properties.min_texture_data_placement_alignment;

    size_t num_uploads = m_gpu_upload_queue.size();

    GpuUploadRecord record{};
    while (num_uploads > 0 && m_gpu_upload_queue.pop(record))
    {
        const uint32_t num_mips = record.metadata.num_mips;
        const int32_t delta = record.request.level_delta;

        const uint32_t old_first_mip = num_mips - record.request.resident_level;
        const uint32_t new_first_mip = num_mips - record.request.desired_level;

        const uint32_t streamed_count = delta > 0 ? static_cast<uint32_t>(delta) : 0;

        uint64_t frame_alloc_size = 0;
        const std::vector<MipUploadLayout> layouts = compute_mips_upload_layout(
            record.metadata, new_first_mip, streamed_count, row_pitch_alignment, placement_alignment, frame_alloc_size);

        ImageResource* new_texture = record.new_texture;
        ImageResource* old_texture = record.old_texture;

        const bool has_old = old_texture != nullptr;

        command.transition_resource(*new_texture, ImageResourceState::ShaderReadOnly, ImageResourceState::TransferDst);

        if (has_old)
        {
            command.transition_resource(
                *old_texture, ImageResourceState::ShaderReadOnly, ImageResourceState::TransferSrc);

            const uint32_t shared_first_mip = std::max(old_first_mip, new_first_mip);
            for (uint32_t mip = shared_first_mip; mip < num_mips; ++mip)
            {
                const glm::uvec2 dims = compute_mip_size(record.metadata.width, record.metadata.height, mip);

                const CopyImageToImageInfo info{
                    .source_offset = {0, 0, 0},
                    .source_subresource_layers =
                        {
                            .mip_level = mip - old_first_mip,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                    .dest_offset = {0, 0, 0},
                    .dest_subresource_layers =
                        {
                            .mip_level = mip - new_first_mip,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                    .extent = {dims.x, dims.y, record.metadata.depth},
                };

                command.copy_image_to_image(*old_texture, *new_texture, info);
            }
        }

        if (streamed_count > 0)
        {
            MIZU_ASSERT(record.staging_block.is_valid(), "Upgrade without staged data");

            const FrameAllocation allocation =
                frame_allocator.allocate(frame_alloc_size, placement_alignment, sizeof(uint8_t));

            uint8_t* mapped = allocation.get_mapped_data();
            const uint8_t* src = record.staging_block.as_span().data();
            const uint32_t format_size = get_image_format_size(record.metadata.format);

            for (uint32_t i = 0; i < streamed_count; ++i)
            {
                const MipUploadLayout& layout = layouts[i];

                const uint64_t src_row_bytes = static_cast<uint64_t>(layout.dims.x) * format_size;
                const uint64_t rows = static_cast<uint64_t>(layout.dims.y) * record.metadata.depth;

                if (layout.row_pitch == src_row_bytes)
                {
                    memcpy(mapped + layout.dst_offset, src + layout.src_offset, src_row_bytes * rows);
                }
                else
                {
                    for (uint64_t row = 0; row < rows; ++row)
                    {
                        memcpy(
                            mapped + layout.dst_offset + row * layout.row_pitch,
                            src + layout.src_offset + row * src_row_bytes,
                            src_row_bytes);
                    }
                }

                const CopyBufferToImageInfo info{
                    .buffer_offset = allocation.view.desc.offset + layout.dst_offset,
                    .buffer_row_length = layout.row_pitch / format_size,
                    .image_subresource_layers =
                        {
                            .mip_level = i,
                            .base_array_layer = 0,
                            .layer_count = 1,
                        },
                    .image_offset = {0, 0, 0},
                    .image_extent = {layout.dims.x, layout.dims.y, record.metadata.depth},
                };

                command.copy_buffer_to_image(*frame_allocator.get_buffer(), *new_texture, info);
            }
        }

        command.transition_resource(*new_texture, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);

        if (has_old)
        {
            command.transition_resource(
                *old_texture, ImageResourceState::TransferSrc, ImageResourceState::ShaderReadOnly);
        }

        if (record.staging_block.is_valid())
        {
            m_staging_pool.free(record.staging_block);
        }

        if (has_old)
        {
            m_texture_pool.free(old_texture, EVICTION_FRAMES);
        }

        TextureResidencyPayload* payload = m_residency.get_payload(record.handle);
        payload->resident_level = record.request.desired_level;

        if (!m_residency.transition_status(record.handle, ResidencyStatus::Loading, ResidencyStatus::GpuResident))
        {
            MIZU_UNREACHABLE("Failed to transition handle state when finalizing an upload");
        }

        m_texture_updates.push_back({
            .handle = record.handle,
            .new_texture = new_texture,
        });
    }
}

void TextureStreamingManager::add_usage(const TextureStreamingUsage& usage)
{
    const auto it = m_usages_map.find(usage.streaming_handle);
    if (it != m_usages_map.end())
    {
        MIZU_UNREACHABLE(
            "Trying to add usage with streaming handle ({}) that already exists", usage.streaming_handle.id);
        return;
    }

    if (!m_residency.increment_reference_count(usage.texture_handle))
    {
        MIZU_UNREACHABLE("Failed to increment reference count");
        return;
    }

    m_usages_map.emplace(usage.streaming_handle, usage);

    auto texture_to_usages_it = m_texture_to_usages_map.find(usage.texture_handle);
    if (texture_to_usages_it == m_texture_to_usages_map.end())
    {
        texture_to_usages_it = m_texture_to_usages_map.insert({usage.texture_handle, {}}).first;

        if (is_streamable(usage.texture_handle))
        {
            m_streamable_textures.push_back(usage.texture_handle);
        }
    }

    texture_to_usages_it->second.push_back(usage);

    m_dirty_handles.push_back(usage.texture_handle);
}

void TextureStreamingManager::remove_usage(TextureStreamingHandle streaming_handle)
{
    const auto it = m_usages_map.find(streaming_handle);
    if (it == m_usages_map.end())
    {
        MIZU_UNREACHABLE("Trying to remove usage with owner handle ({}) that does not exist", streaming_handle.id);
        return;
    }

    const TextureStreamingUsage& usage = it->second;
    if (!m_residency.decrement_reference_count(usage.texture_handle))
    {
        MIZU_UNREACHABLE("Failed to decrement reference count");
        return;
    }

    const auto texture_to_usages_it = m_texture_to_usages_map.find(usage.texture_handle);
    MIZU_ASSERT(texture_to_usages_it != m_texture_to_usages_map.end(), "Failed to find texture to usage entry");

    std::erase_if(texture_to_usages_it->second, [&usage](const TextureStreamingUsage& u) {
        return u.streaming_handle == usage.streaming_handle;
    });

    if (texture_to_usages_it->second.empty())
    {
        if (is_streamable(usage.texture_handle))
        {
            std::erase_if(m_streamable_textures, [&usage](const TextureAssetHandle& handle) {
                return handle == usage.texture_handle;
            });
        }

        m_texture_to_usages_map.erase(texture_to_usages_it);
    }

    m_dirty_handles.push_back(usage.texture_handle);
}

void TextureStreamingManager::refresh(TextureAssetHandle handle, const StreamingContext& context)
{
    MIZU_PROFILE_SCOPED;

    const ResidencyStatus status = m_residency.get_status(handle);

    if (status == ResidencyStatus::Loading)
    {
        // If currently streaming, ignore and defer to a next frame to prevent race conditions
        return;
    }

    // Check if there is already a pending request for this texture handle
    // TODO: Could investigate in the future if there is a better approach, like joining duplicated handles into a
    // single, max, request
    if (has_pending_request_for_handle(handle))
    {
        return;
    }

    const uint32_t references = m_residency.get_references(handle);
    const TextureResidencyPayload* payload = m_residency.get_payload(handle);
    MIZU_ASSERT(payload != nullptr, "Failed to get payload for texture handle: {}", handle.get_id());

    const std::optional<EvaluateResult> result = evaluate(handle, context, *payload, references);
    if (!result.has_value())
        return;

    if (result->desired_level != payload->resident_level)
    {
        const int8_t delta = static_cast<int8_t>(result->desired_level) - static_cast<int8_t>(payload->resident_level);
        m_pending_requests.push_back({
            .manager = this,
            .asset_key = to_asset_key(handle),
            .resident_level = payload->resident_level,
            .desired_level = result->desired_level,
            .level_delta = delta,
            .score = result->score,
        });
    }
}

auto TextureStreamingManager::evaluate(
    TextureAssetHandle handle,
    const StreamingContext& context,
    const TextureResidencyPayload& payload,
    uint32_t references) -> std::optional<EvaluateResult>
{
    constexpr float EVICTION_SCORE = 5.0f;
    constexpr float FIRST_LOAD_SCORE = 10.0f;

    const std::optional<TextureAssetRecord> record = m_asset_loader.get_texture_record(handle);
    MIZU_ASSERT(record.has_value(), "Failed to get texture record for streaming handle: {}", handle.get_id());

    const TextureAssetMetadata& metadata = record->metadata;

    // If it has no references, means that it has no further usages. Request eviction.
    if (references == 0)
    {
        return EvaluateResult{
            .desired_level = 0,
            .score = EVICTION_SCORE,
        };
    }

    const bool streamable = is_streamable(handle);
    const uint8_t max_level = static_cast<uint8_t>(metadata.num_mips);

    // If it's not streamable, and the resident level is 0, it means it's the first load of the asset, and because
    // we don't have to take into account which quality to load, just request the load.
    if (!streamable && payload.resident_level == 0)
    {
        return EvaluateResult{
            .desired_level = max_level,
            .score = FIRST_LOAD_SCORE,
        };
    }

    if (!streamable)
        return std::nullopt;

    const uint32_t texture_dim = glm::max(metadata.width, metadata.height);

    const int32_t mip_tail_start =
        static_cast<int32_t>(glm::min<uint32_t>(metadata.streamable_mips, metadata.num_mips - 1));
    // TODO: Kept as a variable in case in the future we add the ability to "skip mips"
    const int32_t finest_top_mip = 0;

    const TransformRegistry& transform_registry = transform_registry_get();

    const auto usages_it = m_texture_to_usages_map.find(handle);
    if (usages_it == m_texture_to_usages_map.end())
    {
        MIZU_UNREACHABLE("Failed to find usages for texture handle: {}", handle.get_id());
        return std::nullopt;
    }

    const std::vector<TextureStreamingUsage>& usages = usages_it->second;

    uint8_t desired_level = 0;
    float score = 0.0f;

    for (const TextureStreamingUsage& usage : usages)
    {
        if (desired_level == max_level)
        {
            // We're already registering the max quality level, no need to evaluate other usages
            break;
        }

        const TransformInfo& transform_info = transform_registry.get_transform_info(usage.transform_handle);

        const float scale = math::max_axis_scale(transform_info.transform);
        const math::Sphere sphere = bounding_sphere_from_transformed_aabb(usage.aabb, transform_info.transform);

        for (const RenderViewRegistryEntry& view : context.views)
        {
            const float center_depth = -(view.view_matrix * glm::vec4(sphere.center, 1.0f)).z;
            const float depth = glm::max(center_depth - sphere.radius, view.camera.znear);

            const float pixels_per_uv =
                usage.texel_factor * scale
                * pixels_per_world_unit(view.viewport, view.camera.fov, depth, context.viewport_size);
            const float texels_per_pixel = static_cast<float>(texture_dim) / std::max(pixels_per_uv, 0.0001f);

            constexpr float MIPS_BIAS = 0.5f;
            const float lod = std::log2(texels_per_pixel) - MIPS_BIAS;
            if (std::isnan(lod))
                continue;

            const float clamped =
                glm::clamp(lod, static_cast<float>(finest_top_mip), static_cast<float>(mip_tail_start));
            const int32_t top_mip = static_cast<int32_t>(std::floor(clamped));

            const uint8_t desired = static_cast<uint8_t>(metadata.num_mips - static_cast<uint32_t>(top_mip));
            const float coverage = projected_area_fraction(sphere, view, context.viewport_size);

            desired_level = glm::max(desired_level, desired);
            score = glm::max(score, coverage);
        }
    }

    if (payload.resident_level == 0)
        score += FIRST_LOAD_SCORE;

    return EvaluateResult{
        .desired_level = desired_level,
        .score = score,
    };
}

bool TextureStreamingManager::has_pending_request_for_handle(TextureAssetHandle handle) const
{
    for (const StreamingRequest& req : m_pending_requests)
    {
        const TextureAssetHandle req_handle = to_asset_handle<TextureAssetHandle>(req.asset_key);
        if (req_handle == handle)
        {
            return true;
        }
    }
    return false;
}

bool TextureStreamingManager::is_streamable(TextureAssetHandle handle) const
{
    if (const std::optional<TextureAssetRecord> record = m_asset_loader.get_texture_record(handle); record.has_value())
    {
        const TextureAssetMetadata& metadata = record->metadata;
        return metadata.streamable_mips > 0 && metadata.num_mips > 1;
    }

    MIZU_UNREACHABLE("Failed to get record for TextureAssetHandle: {}", handle.get_id());
    return false;
}

//
// TexturePool
//

ImageResource* TexturePool::allocate(const ImageDescription& desc)
{
    const auto image = g_render_device->create_image(desc);

    ImageResource* ptr = image.get();
    const uint64_t id = (uint64_t)ptr;

    m_textures_map.emplace(id, image);

    return ptr;
}

void TexturePool::free(ImageResource* resource, uint32_t eviction_frames)
{
    const uint64_t id = (uint64_t)resource;

    const auto it = m_textures_map.find(id);
    if (it == m_textures_map.end())
    {
        MIZU_UNREACHABLE("Failed to release image resource");
        return;
    }

    m_evictions.push_back({
        .id = id,
        .frames = eviction_frames,
    });
}

void TexturePool::track_evictions()
{
    auto it = m_evictions.begin();
    while (it != m_evictions.end())
    {
        it->frames -= 1;

        if (it->frames == 0)
        {
            const auto textures_it = m_textures_map.find(it->id);
            // Resetting because the TexturePool owns the textures
            textures_it->second.reset();

            m_textures_map.erase(textures_it);

            it = m_evictions.erase(it);
        }
    }
}

} // namespace Mizu
