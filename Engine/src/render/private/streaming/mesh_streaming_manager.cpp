#include "streaming/mesh_streaming_manager.h"

#include <cmath>

#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "base/math/math.h"
#include "base/math/shapes.h"
#include "render_core/rhi/buffer_resource.h"

#include "registries/render_view_registry.h"
#include "registries/transform_registry.h"
#include "render/runtime/renderer.h"
#include "render/systems/frame_linear_allocator.h"
#include "resources/asset_load_scheduler.h"
#include "resources/gpu_pools.h"

namespace Mizu
{

static constexpr uint32_t FRAMES_TO_UPDATE = 3u;

MeshStreamingManager::MeshStreamingManager(
    IAssetLoader& asset_loader,
    AssetLoadScheduler& load_scheduler,
    StagingPool& staging_pool,
    GpuMeshPool& gpu_mesh_pool)
    : m_asset_loader(asset_loader)
    , m_load_scheduler(load_scheduler)
    , m_staging_pool(staging_pool)
    , m_gpu_mesh_pool(gpu_mesh_pool)
{
}

void MeshStreamingManager::update(const StreamingContext& context)
{
    MIZU_PROFILE_SCOPED;

    m_mesh_updates.clear();

    if (m_update_cursor >= FRAMES_TO_UPDATE)
    {
        m_update_cursor = 0;
    }

    if (m_update_cursor == 0)
    {
        m_iterations_per_update = math::ceil_div(static_cast<uint32_t>(m_streamable_meshes.size()), FRAMES_TO_UPDATE);
    }

    for (MeshAssetHandle handle : m_dirty_handles)
    {
        refresh(handle, context);
    }

    m_dirty_handles.clear();

    for (uint32_t i = 0; i < m_iterations_per_update; ++i)
    {
        const uint32_t streamable_idx = m_update_cursor * m_iterations_per_update + i;
        if (streamable_idx >= m_streamable_meshes.size())
            break;

        const MeshAssetHandle handle = m_streamable_meshes[streamable_idx];
        refresh(handle, context);
    }

    m_update_cursor += 1;
}

void MeshStreamingManager::gather(std::vector<StreamingRequest>& requests)
{
    for (const StreamingRequest& pending : m_pending_requests)
    {
        requests.push_back(pending);
    }

    m_pending_requests.clear();
}

void MeshStreamingManager::execute(std::span<StreamingAward> awards)
{
    for (const StreamingAward& award : awards)
    {
        const StreamingRequest& request = award.request;

        const MeshAssetHandle handle = to_asset_handle<MeshAssetHandle>(request.asset_key);
        // TODO: Please cache this value please :(
        const MeshAssetMetadata metadata = m_asset_loader.get_mesh_record(handle)->metadata;

        MeshResidencyPayload* payload = m_residency.get_payload(handle);
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

            m_gpu_mesh_pool.free(payload->residency.allocation_handle);

            m_mesh_updates.push_back({
                .handle = handle,
                .residency = {},
                .resident = false,
            });

            continue;
        }

        const uint32_t first_lod = metadata.num_lods - request.desired_level;
        const int32_t lod_delta =
            static_cast<int32_t>(request.desired_level) - static_cast<int32_t>(request.resident_level);

        MIZU_LOG_INFO(
            "Streaming handle={}, first_lod={}, lod_delta={}, desired_level={}, resident_level={}",
            request.asset_key.id,
            first_lod,
            lod_delta,
            request.desired_level,
            request.resident_level);

        // TODO: Lods aren't really implemented yet, for the moment keep simple and load the whole mesh
        MIZU_ASSERT(metadata.num_lods == 1, "Lods aren't really implemented yet");

        const std::optional<StagingBlock> block =
            m_staging_pool.allocate(metadata.get_total_size_bytes(), metadata.get_total_alignment_bytes());
        if (!block.has_value())
        {
            MIZU_ASSERT(false, "Failed to allocate from StagingPool");
            continue;
        }

        const std::optional<GpuMeshAllocationHandle> allocation = m_gpu_mesh_pool.allocate(
            handle,
            metadata.get_vertex_data_size_bytes(),
            metadata.get_index_data_size_bytes(),
            metadata.get_vertex_alignment_bytes(),
            metadata.get_index_alignment_bytes());
        if (!allocation.has_value())
        {
            MIZU_UNREACHABLE("Failed to allocate mesh with handle: {}", handle.get_id());
            continue;
        }

        payload->residency = MeshStreamingResidency{
            .allocation_handle = *allocation,
            .metadata = metadata,
        };

        const ResidencyStatus expected_status =
            payload->resident_level == 0 ? ResidencyStatus::Unloaded : ResidencyStatus::GpuResident;
        if (!m_residency.transition_status(handle, expected_status, ResidencyStatus::Loading))
        {
            MIZU_UNREACHABLE("Failed to transition texture when requesting a load");
            continue;
        }

        /* TODO:
        if (lod_delta < 0)
        {
            // Streaming out case

            m_gpu_upload_queue.push({
                // ...
            });

            continue;
        }
        */

        m_load_scheduler.request_mesh_load(
            handle,
            first_lod,
            static_cast<uint32_t>(lod_delta),
            block->as_span(),
            [this, metadata, request, allocation, block](MeshAssetHandle handle) {
                m_gpu_upload_queue.push({
                    .handle = handle,
                    .metadata = metadata,
                    .request = request,
                    .allocation = *allocation,
                    .staging_block = *block,
                });
            });
    }
}

void MeshStreamingManager::record_gpu_uploads(CommandBuffer& command, FrameLinearAllocator& frame_allocator)
{
    size_t num_uploads = m_gpu_upload_queue.size();

    GpuUploadRecord record{};
    while (num_uploads > 0 && m_gpu_upload_queue.pop(record))
    {
        MIZU_ASSERT(record.metadata.num_lods == 1, "Multiple lods currently not supported");

        BufferResource& vertex_buffer = *m_gpu_mesh_pool.get_vertex_buffer();
        BufferResource& index_buffer = *m_gpu_mesh_pool.get_index_buffer();

        const uint64_t total_size = record.metadata.get_total_size_bytes();
        const uint64_t alignment = g_render_device->get_properties().min_raw_buffer_offset_alignment;

        const FrameAllocation allocation = frame_allocator.allocate(total_size, alignment, sizeof(uint8_t));
        allocation.upload(record.staging_block.as_span());

        const CopyBufferToBufferInfo vertex_copy_info{
            .size = record.metadata.get_vertex_data_size_bytes(),
            .src_offset = allocation.view.desc.offset + record.metadata.vertex_data_offset,
            .dst_offset = record.allocation.vertex_offset,
        };

        command.copy_buffer_to_buffer(*frame_allocator.get_buffer(), vertex_buffer, vertex_copy_info);

        const CopyBufferToBufferInfo index_copy_info{
            .size = record.metadata.get_index_data_size_bytes(),
            .src_offset = allocation.view.desc.offset + record.metadata.index_data_offset,
            .dst_offset = record.allocation.index_offset,
        };

        command.copy_buffer_to_buffer(*frame_allocator.get_buffer(), index_buffer, index_copy_info);

        if (record.staging_block.is_valid())
        {
            m_staging_pool.free(record.staging_block);
        }

        MeshResidencyPayload* payload = m_residency.get_payload(record.handle);
        payload->resident_level = record.request.desired_level;

        if (!m_residency.transition_status(record.handle, ResidencyStatus::Loading, ResidencyStatus::GpuResident))
        {
            MIZU_UNREACHABLE("Failed to transition handle state when finalizing an upload");
        }

        m_mesh_updates.push_back({
            .handle = record.handle,
            .residency =
                MeshStreamingResidency{
                    .allocation_handle = record.allocation,
                    .metadata = record.metadata,
                },
            .resident = true,
        });
    }
}

void MeshStreamingManager::add_usage(const MeshStreamingUsage& usage)
{
    const auto it = m_usages_map.find(usage.streaming_handle);
    if (it != m_usages_map.end())
    {
        MIZU_UNREACHABLE(
            "Trying to add usage with streaming handle ({}) that already exists", usage.streaming_handle.id);
        return;
    }

    if (!m_residency.increment_reference_count(usage.mesh_handle))
    {
        MIZU_UNREACHABLE("Failed to increment reference count");
        return;
    }

    m_usages_map.emplace(usage.streaming_handle, usage);

    auto mesh_to_usages_it = m_mesh_to_usages_map.find(usage.mesh_handle);
    if (mesh_to_usages_it == m_mesh_to_usages_map.end())
    {
        mesh_to_usages_it = m_mesh_to_usages_map.insert({usage.mesh_handle, {}}).first;

        if (is_streamable(usage.mesh_handle))
        {
            m_streamable_meshes.push_back(usage.mesh_handle);
        }
    }

    mesh_to_usages_it->second.push_back(usage);

    m_dirty_handles.push_back(usage.mesh_handle);
}

void MeshStreamingManager::remove_usage(MeshStreamingHandle streaming_handle)
{
    const auto it = m_usages_map.find(streaming_handle);
    if (it == m_usages_map.end())
    {
        MIZU_UNREACHABLE("Trying to remove usage with owner handle ({}) that does not exist", streaming_handle.id);
        return;
    }

    const MeshStreamingUsage& usage = it->second;
    if (!m_residency.decrement_reference_count(usage.mesh_handle))
    {
        MIZU_UNREACHABLE("Failed to decrement reference count");
        return;
    }

    const auto mesh_to_usages_it = m_mesh_to_usages_map.find(usage.mesh_handle);
    MIZU_ASSERT(mesh_to_usages_it != m_mesh_to_usages_map.end(), "Failed to find mesh to usage entry");

    std::erase_if(mesh_to_usages_it->second, [&usage](const MeshStreamingUsage& u) {
        return u.streaming_handle == usage.streaming_handle;
    });

    if (mesh_to_usages_it->second.empty())
    {
        if (is_streamable(usage.mesh_handle))
        {
            std::erase_if(
                m_streamable_meshes, [&usage](const MeshAssetHandle& handle) { return handle == usage.mesh_handle; });
        }

        m_mesh_to_usages_map.erase(mesh_to_usages_it);
    }

    m_dirty_handles.push_back(usage.mesh_handle);
}

std::optional<MeshStreamingResidency> MeshStreamingManager::get_mesh_residency(MeshAssetHandle handle) const
{
    const MeshResidencyPayload* payload = m_residency.get_payload(handle);
    if (payload == nullptr)
        return std::nullopt;

    return payload->residency;
}

bool MeshStreamingManager::is_drawable(MeshAssetHandle handle) const
{
    // TODO: This could not be the best drawable metric, imagine a mesh that is already resident, but we're streaming in
    // a new lod, it would be in the Loading status
    return m_residency.get_status(handle) == ResidencyStatus::GpuResident;
}

void MeshStreamingManager::refresh(MeshAssetHandle handle, const StreamingContext& context)
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
    const MeshResidencyPayload* payload = m_residency.get_payload(handle);
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

static uint32_t lod_for_screen_size(const MeshAssetMetadata& metadata, float screen_size)
{
    for (uint32_t lod = 0; lod + 1 < metadata.num_lods; ++lod)
    {
        if (screen_size >= metadata.lod_screen_sizes[lod])
            return lod;
    }

    return metadata.num_lods - 1;
}

auto MeshStreamingManager::evaluate(
    MeshAssetHandle handle,
    const StreamingContext& context,
    const MeshResidencyPayload& payload,
    uint32_t references) -> std::optional<EvaluateResult>
{
    constexpr float EVICTION_SCORE = 5.0f;
    constexpr float FIRST_LOAD_SCORE = 10.0f;

    const std::optional<MeshAssetRecord> record = m_asset_loader.get_mesh_record(handle);
    MIZU_ASSERT(record.has_value(), "Failed to get mesh record for streaming handle: {}", handle.get_id());

    const MeshAssetMetadata& metadata = record->metadata;

    // If it has no references, means that it has no further usages. Request eviction.
    if (references == 0)
    {
        return EvaluateResult{
            .desired_level = 0,
            .score = EVICTION_SCORE,
        };
    }

    const bool streamable = is_streamable(handle);
    const uint8_t max_level = static_cast<uint8_t>(metadata.num_lods);

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

    const uint32_t tail_lods = metadata.num_lods - glm::min<uint32_t>(metadata.streamable_lods, metadata.num_lods);
    const uint32_t min_level = glm::max(1u, tail_lods);

    // TODO: must be mesh config
    constexpr uint32_t SKIP_LODS = 0;
    const uint32_t finest_lod = glm::min(SKIP_LODS, metadata.num_lods - min_level);

    const TransformRegistry& transform_registry = transform_registry_get();

    const auto usages_it = m_mesh_to_usages_map.find(handle);
    if (usages_it == m_mesh_to_usages_map.end())
    {
        MIZU_UNREACHABLE("Failed to find usages for texture handle: {}", handle.get_id());
        return std::nullopt;
    }

    const std::vector<MeshStreamingUsage>& usages = usages_it->second;

    uint8_t desired_level = 0;
    float score = 0.0f;

    for (const MeshStreamingUsage& usage : usages)
    {
        if (desired_level == max_level)
        {
            // We're already registering the max quality level, no need to evaluate other usages
            break;
        }

        const TransformInfo& transform_info = transform_registry.get_transform_info(usage.transform_handle);
        const math::Sphere sphere =
            math::bounding_sphere_from_transformed_aabb(metadata.bounding_box, transform_info.transform);

        for (const RenderViewRegistryEntry& view : context.views)
        {
            const float center_depth = -(view.view_matrix * glm::vec4(sphere.center, 1.0f)).z;
            // TODO: This is to ignore if it's behind the camera, keeping commented to prevent popping when doing a 180
            // if (center_depth + sphere.radius <= view.camera.znear)
            //     continue;

            const float depth = glm::max(center_depth, view.camera.znear);

            constexpr float LOD_DETAIL_BIAS = 1.25f;

            const float screen_size = LOD_DETAIL_BIAS * sphere.radius / (depth * std::tan(0.5f * view.camera.fov));

            const uint32_t lod = glm::max(lod_for_screen_size(metadata, screen_size), finest_lod);
            const uint8_t desired = static_cast<uint8_t>(metadata.num_lods - lod);

            desired_level = glm::max<uint8_t>(desired_level, desired);
            score = glm::max(score, screen_size);
        }
    }

    if (payload.resident_level == 0)
        score += FIRST_LOAD_SCORE;

    return EvaluateResult{
        .desired_level = desired_level,
        .score = score,
    };
}

bool MeshStreamingManager::has_pending_request_for_handle(MeshAssetHandle handle) const
{
    for (const StreamingRequest& req : m_pending_requests)
    {
        const MeshAssetHandle req_handle = to_asset_handle<MeshAssetHandle>(req.asset_key);
        if (req_handle == handle)
        {
            return true;
        }
    }

    return false;
}

bool MeshStreamingManager::is_streamable(MeshAssetHandle handle) const
{
    if (const std::optional<MeshAssetRecord> record = m_asset_loader.get_mesh_record(handle); record.has_value())
    {
        // TODO: Implement when lods are added to meshes

        (void)record;
        return false;
    }

    MIZU_UNREACHABLE("Failed to get record for MeshAssetHandle: {}", handle.get_id());
    return false;
}

} // namespace Mizu
