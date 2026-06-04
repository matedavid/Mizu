#pragma once

#include "asset/asset_handle.h"
#include "core/job_system/mpsc_queue.h"

#include "resources/resource_event_stream.h"

namespace Mizu
{

enum class StreamingRequestType
{
    Load,
    Evict,
};

struct MeshStreamingRequest
{
    StreamingRequestType type;
    MeshAssetHandle mesh_handle;
};

struct TextureStreamingRequest
{
    StreamingRequestType type;
    TextureAssetHandle texture_handle;
};

struct MaterialStreamingRequest
{
    StreamingRequestType type;
    MaterialAssetHandle material_handle;
};

constexpr size_t MAX_STREAMING_REQUESTS = 1024;

using StreamingMeshRequestQueue = MpscQueue<MeshStreamingRequest, MAX_STREAMING_REQUESTS>;
using StreamingTextureRequestQueue = MpscQueue<TextureStreamingRequest, MAX_STREAMING_REQUESTS>;
using StreamingMaterialRequestQueue = MpscQueue<MaterialStreamingRequest, MAX_STREAMING_REQUESTS>;

struct StreamingPlannerConfig
{
};

class StreamingPlanner
{
  public:
    StreamingPlanner(StreamingPlannerConfig config);

    void update(const ResourceEventStream& stream);

    StreamingMeshRequestQueue& get_mesh_request_queue() { return m_mesh_request_queue; }
    StreamingTextureRequestQueue& get_texture_request_queue() { return m_texture_request_queue; }
    StreamingMaterialRequestQueue& get_material_request_queue() { return m_material_request_queue; }

  private:
    [[maybe_unused]] StreamingPlannerConfig m_config{};

    StreamingMeshRequestQueue m_mesh_request_queue{};
    StreamingTextureRequestQueue m_texture_request_queue{};
    StreamingMaterialRequestQueue m_material_request_queue{};

    void consume_create_delta(const RenderableEvent& event);
    void consume_update_delta(const RenderableEvent& event);
    void consume_destroy_delta(const RenderableEvent& event);
};

} // namespace Mizu