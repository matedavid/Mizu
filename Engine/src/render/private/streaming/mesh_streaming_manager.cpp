#include "streaming/mesh_streaming_manager.h"

#include <algorithm>

#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "base/math/math.h"
#include "base/math/shapes.h"
#include "render_core/rhi/buffer_resource.h"

#include "registries/render_view_registry.h"
#include "registries/transform_registry.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/systems/frame_linear_allocator.h"
#include "resources/asset_load_scheduler.h"
#include "streaming/mesh_lod_utils.h"
#include "streaming/mesh_streaming_pool.h"

namespace Mizu
{

static constexpr uint32_t MESH_FRAMES_TO_UPDATE = 3u;

MeshStreamingManager::MeshStreamingManager(
    IAssetLoader& asset_loader,
    AssetLoadScheduler& load_scheduler,
    StagingPool& staging_pool,
    MeshStreamingPool& mesh_streaming_pool)
    : m_asset_loader(asset_loader)
    , m_load_scheduler(load_scheduler)
    , m_staging_pool(staging_pool)
    , m_mesh_streaming_pool(mesh_streaming_pool)
{
}

void MeshStreamingManager::update(const StreamingContext& context)
{
    MIZU_PROFILE_SCOPED;

    m_mesh_updates.clear();

    if (m_update_cursor >= MESH_FRAMES_TO_UPDATE)
    {
        m_update_cursor = 0;
    }

    if (m_update_cursor == 0)
    {
        m_iterations_per_update =
            math::ceil_div(static_cast<uint32_t>(m_streamable_meshes.size()), MESH_FRAMES_TO_UPDATE);
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
        MIZU_ASSERT(
            award.request.asset_key.type == AssetType::Mesh,
            "Trying to execute award for invalid asset type. Got = {}, Expected = {}",
            meta::enum_name(award.request.asset_key.type),
            meta::enum_name(AssetType::Mesh));

        const StreamingRequest& request = award.request;

        const MeshAssetHandle handle = to_asset_handle<MeshAssetHandle>(request.asset_key);

        const MeshEntry* entry = find_entry(handle);
        if (entry == nullptr)
        {
            MIZU_UNREACHABLE("Failed to find mesh entry for handle: {}", handle.get_id());
            continue;
        }
        const MeshAssetMetadata& metadata = entry->metadata;

        MeshResidencyPayload* payload = m_residency.get_payload(handle);
        if (payload == nullptr)
        {
            MIZU_UNREACHABLE("Failed to get payload");
            continue;
        }

        if (request.desired_level == 0)
        {
            if (!m_residency.transition_status(handle, ResidencyStatus::GpuResident, ResidencyStatus::Unloaded))
            {
                MIZU_UNREACHABLE("Failed to transition resource when evicting");
                continue;
            }

            payload->resident_level = 0;
            payload->residency = {};

            m_mesh_streaming_pool.update_residency_state(handle, metadata, metadata.get_num_lods());

            m_mesh_updates.push_back({
                .handle = handle,
                .residency = {},
                .resident = false,
            });

            if (m_residency.get_references(handle) == 0)
            {
                unregister_mesh(handle);
            }

            continue;
        }

        const uint8_t first_lod = metadata.get_num_lods() - request.desired_level;
        const int32_t lod_delta =
            static_cast<int32_t>(request.desired_level) - static_cast<int32_t>(request.resident_level);

        MIZU_LOG_INFO(
            "Streaming handle={}, first_lod={}, lod_delta={}, desired_level={}, resident_level={}",
            request.asset_key.id,
            first_lod,
            lod_delta,
            request.desired_level,
            request.resident_level);

        StagingBlock block;

        if (lod_delta > 0)
        {
            const uint64_t size = metadata.get_lod_range_file_size_bytes(first_lod, static_cast<uint8_t>(lod_delta));

            const std::optional<StagingBlock> block_opt = m_staging_pool.allocate(size, MESH_FILE_SEGMENT_ALIGNMENT);
            if (!block_opt.has_value())
            {
                MIZU_ASSERT(false, "Failed to allocate from StagingPool");
                continue;
            }

            block = block_opt.value();
        }

        if (!m_mesh_streaming_pool.update_residency_state(handle, metadata, first_lod))
        {
            if (block.is_valid())
            {
                m_staging_pool.free(block);
            }

            continue;
        }

        const ResidencyStatus expected_status =
            payload->resident_level == 0 ? ResidencyStatus::Unloaded : ResidencyStatus::GpuResident;
        if (!m_residency.transition_status(handle, expected_status, ResidencyStatus::Loading))
        {
            MIZU_UNREACHABLE("Failed to transition mesh when requesting a load");
            continue;
        }

        if (lod_delta < 0)
        {
            m_gpu_upload_queue.push({
                .handle = handle,
                .metadata = metadata,
                .request = request,
                .staging_block = block,
            });

            continue;
        }

        m_load_scheduler.request_mesh_load(
            handle,
            first_lod,
            static_cast<uint8_t>(lod_delta),
            block.as_span(),
            [this, metadata, request, block](MeshAssetHandle handle) {
                m_gpu_upload_queue.push({
                    .handle = handle,
                    .metadata = metadata,
                    .request = request,
                    .staging_block = block,
                });
            });
    }
}

void MeshStreamingManager::setup_render_graph_upload_pass(RenderGraphBuilder& builder, RenderGraphPassBuilder& pass)
{
    const RenderGraphResource mesh_gpu_vertex_buffer = builder.register_external_buffer(
        m_mesh_streaming_pool.get_vertex_buffer(),
        {.initial_state = BufferResourceState::ShaderReadOnly, .final_state = BufferResourceState::ShaderReadOnly});
    const RenderGraphResource mesh_gpu_index_buffer = builder.register_external_buffer(
        m_mesh_streaming_pool.get_index_buffer(),
        {.initial_state = BufferResourceState::ShaderReadOnly, .final_state = BufferResourceState::ShaderReadOnly});

    pass.copy_dst(mesh_gpu_vertex_buffer);
    pass.copy_dst(mesh_gpu_index_buffer);
}

void MeshStreamingManager::record_gpu_uploads(CommandBuffer& command, FrameLinearAllocator& frame_allocator)
{
    size_t num_uploads = m_gpu_upload_queue.size();

    GpuUploadRecord record{};
    while (num_uploads > 0 && m_gpu_upload_queue.pop(record))
    {
        num_uploads -= 1;

        command.begin_gpu_marker("MeshStreamingManager::MeshUpload");

        const uint8_t num_lods = record.metadata.get_num_lods();

        const MeshUploadData upload{
            .old_first_lod = static_cast<uint8_t>(num_lods - record.request.resident_level),
            .new_first_lod = static_cast<uint8_t>(num_lods - record.request.desired_level),
            .data = record.staging_block.is_valid() ? std::span<const uint8_t>(record.staging_block.as_span())
                                                    : std::span<const uint8_t>{},
        };

        m_mesh_streaming_pool.record_gpu_upload(record.handle, record.metadata, upload, command, frame_allocator);

        if (record.staging_block.is_valid())
        {
            m_staging_pool.free(record.staging_block);
        }

        const std::optional<GpuMeshAllocationHandle> allocation = m_mesh_streaming_pool.get_allocation(record.handle);
        MIZU_ASSERT(allocation.has_value(), "Failed to get allocation for mesh {}", record.handle.get_id());

        const uint8_t resident_first_lod = record.metadata.get_num_lods() - record.request.desired_level;
        const MeshStreamingResidency residency{
            .allocation_handle = *allocation,
            .metadata = record.metadata,
            .resident_first_lod = resident_first_lod,
        };

        MeshResidencyPayload* payload = m_residency.get_payload(record.handle);
        payload->resident_level = record.request.desired_level;
        payload->residency = residency;

        if (!m_residency.transition_status(record.handle, ResidencyStatus::Loading, ResidencyStatus::GpuResident))
        {
            MIZU_UNREACHABLE("Failed to transition handle state when finalizing an upload");
            continue;
        }

        m_mesh_updates.push_back({
            .handle = record.handle,
            .residency = residency,
            .resident = true,
        });

        command.end_gpu_marker();
    }
}

void MeshStreamingManager::add_usage(const MeshStreamingUsage& usage)
{
    if (m_usage_to_mesh.contains(usage.streaming_handle))
    {
        MIZU_UNREACHABLE(
            "Trying to add usage with streaming handle ({}) that already exists", usage.streaming_handle.id);
        return;
    }

    MeshEntry* entry = register_mesh(usage.mesh_handle);
    if (entry == nullptr)
        return;

    if (!m_residency.increment_reference_count(usage.mesh_handle))
    {
        MIZU_UNREACHABLE("Failed to increment reference count");
        return;
    }

    if (entry->usages.empty() && entry->is_streamable())
    {
        m_streamable_meshes.push_back(usage.mesh_handle);
    }

    entry->usages.push_back(usage);
    m_usage_to_mesh.emplace(usage.streaming_handle, usage.mesh_handle);

    m_dirty_handles.push_back(usage.mesh_handle);
}

void MeshStreamingManager::remove_usage(MeshStreamingHandle streaming_handle)
{
    const auto it = m_usage_to_mesh.find(streaming_handle);
    if (it == m_usage_to_mesh.end())
    {
        MIZU_UNREACHABLE("Trying to remove usage with owner handle ({}) that does not exist", streaming_handle.id);
        return;
    }

    const MeshAssetHandle mesh_handle = it->second;

    if (!m_residency.decrement_reference_count(mesh_handle))
    {
        MIZU_UNREACHABLE("Failed to decrement reference count");
        return;
    }

    m_usage_to_mesh.erase(it);

    MeshEntry* entry = find_entry(mesh_handle);
    MIZU_ASSERT(entry != nullptr, "Failed to find mesh entry");

    std::vector<MeshStreamingUsage>& usages = entry->usages;

    const auto usage_it = std::ranges::find_if(
        usages, [&](const MeshStreamingUsage& u) { return u.streaming_handle == streaming_handle; });
    MIZU_ASSERT(usage_it != usages.end(), "Failed to find usage in mesh entry");

    if (usage_it != std::prev(usages.end()))
    {
        *usage_it = usages.back();
    }
    usages.pop_back();

    if (usages.empty() && entry->is_streamable())
    {
        std::erase(m_streamable_meshes, mesh_handle);
    }

    m_dirty_handles.push_back(mesh_handle);
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
    const MeshResidencyPayload* payload = m_residency.get_payload(handle);
    return payload != nullptr && payload->resident_level > 0;
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
    MIZU_ASSERT(payload != nullptr, "Failed to get payload for mesh handle: {}", handle.get_id());

    if (references == 0 && payload->resident_level == 0)
    {
        unregister_mesh(handle);
        return;
    }

    const MeshEntry* entry = find_entry(handle);
    if (entry == nullptr)
    {
        MIZU_UNREACHABLE("Failed to find mesh entry for handle: {}", handle.get_id());
        return;
    }

    const std::optional<EvaluateResult> result = evaluate(*entry, context, *payload, references);
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

static uint8_t select_mesh_lod(const MeshAssetMetadata& metadata, float screen_size)
{
    const uint8_t num_lods = metadata.get_num_lods();

    for (uint8_t lod = 0; lod + 1 < num_lods; ++lod)
    {
        if (screen_size >= metadata.lods[lod].screen_size)
            return lod;
    }

    return num_lods - 1;
}

auto MeshStreamingManager::evaluate(
    const MeshEntry& entry,
    const StreamingContext& context,
    const MeshResidencyPayload& payload,
    uint32_t references) -> std::optional<EvaluateResult>
{
    constexpr float EVICTION_SCORE = 5.0f;
    constexpr float FIRST_LOAD_SCORE = 10.0f;

    const MeshAssetMetadata& metadata = entry.metadata;

    // If it has no references, means that it has no further usages. Request eviction.
    if (references == 0)
    {
        return EvaluateResult{
            .desired_level = 0,
            .score = EVICTION_SCORE,
        };
    }

    const bool streamable = entry.is_streamable();
    const uint8_t num_lods = metadata.get_num_lods();
    const uint8_t max_level = num_lods;

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

    const uint8_t tail_lods = num_lods - glm::min(metadata.streamable_lods, num_lods);
    const uint8_t min_level = glm::max<uint8_t>(1, tail_lods);

    // TODO: must be mesh config
    constexpr uint8_t SKIP_LODS = 0;
    const uint8_t finest_lod = glm::min<uint8_t>(SKIP_LODS, num_lods - min_level);

    const TransformRegistry& transform_registry = transform_registry_get();

    uint8_t desired_level = 0;
    float score = 0.0f;

    for (const MeshStreamingUsage& usage : entry.usages)
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
            const MeshLodView lod_view = create_mesh_lod_view(view.view_matrix, view.proj_matrix, view.camera.znear);

            // TODO: This is to ignore if it's behind the camera, keeping commented to prevent popping when doing a 180
            // const float center_depth = glm::dot(sphere.center - lod_view.position, lod_view.forward);
            // if (center_depth + sphere.radius <= lod_view.znear)
            //     continue;

            constexpr float LOD_DETAIL_BIAS = 1.25f;

            const float screen_size = LOD_DETAIL_BIAS * compute_mesh_screen_size(sphere, lod_view);

            const uint8_t selected_lod = select_mesh_lod(metadata, screen_size);

            const uint8_t lod = glm::max(selected_lod, finest_lod);
            const uint8_t desired = num_lods - lod;

            desired_level = glm::max<uint8_t>(desired_level, desired);
            score = glm::max(score, screen_size);
        }
    }

    desired_level = glm::max(desired_level, min_level);

    if (payload.resident_level == 0)
        score += FIRST_LOAD_SCORE;

    return EvaluateResult{
        .desired_level = desired_level,
        .score = score,
    };
}

auto MeshStreamingManager::find_entry(MeshAssetHandle handle) -> MeshEntry*
{
    const auto it = m_meshes.find(handle);
    return it != m_meshes.end() ? &it->second : nullptr;
}

auto MeshStreamingManager::register_mesh(MeshAssetHandle handle) -> MeshEntry*
{
    if (MeshEntry* entry = find_entry(handle))
        return entry;

    // The only place we hit the asset loader (which may read from disk)
    const std::optional<MeshAssetRecord> record = m_asset_loader.get_mesh_record(handle);
    if (!record.has_value())
    {
        MIZU_UNREACHABLE("Failed to get record for MeshAssetHandle: {}", handle.get_id());
        return nullptr;
    }

    if (!m_mesh_streaming_pool.register_mesh(handle, record->metadata))
    {
        MIZU_LOG_ERROR("Failed to register mesh in MeshStreamingPool: {}", handle.get_id());
        return nullptr;
    }

    return &m_meshes.emplace(handle, MeshEntry{.metadata = record->metadata}).first->second;
}

void MeshStreamingManager::unregister_mesh(MeshAssetHandle handle)
{
    const auto it = m_meshes.find(handle);
    if (it == m_meshes.end())
        return;

    m_mesh_streaming_pool.unregister_mesh(handle);
    m_meshes.erase(it);
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

} // namespace Mizu