#pragma once

#include "core/job_system/mpsc_queue.h"

#include "registries/renderable_registry.h"

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

constexpr size_t MaxStreamingRequests = 1024;

using StreamingMeshRequestQueue = MpscQueue<MeshStreamingRequest, MaxStreamingRequests>;
using StreamingTextureRequestQueue = MpscQueue<TextureStreamingRequest, MaxStreamingRequests>;
using StreamingMaterialRequestQueue = MpscQueue<MaterialStreamingRequest, MaxStreamingRequests>;

struct StreamingPlannerConfig
{
};

class StreamingPlanner
{
  public:
    StreamingPlanner(StreamingPlannerConfig config, RenderableRegistry& renderable_registry);

    void update();

    StreamingMeshRequestQueue& get_mesh_request_queue() { return m_mesh_request_queue; }
    StreamingTextureRequestQueue& get_texture_request_queue() { return m_texture_request_queue; }
    StreamingMaterialRequestQueue& get_material_request_queue() { return m_material_request_queue; }

  private:
    [[maybe_unused]] StreamingPlannerConfig m_config{};
    RenderableRegistry& m_renderable_registry;

    StreamingMeshRequestQueue m_mesh_request_queue{};
    StreamingTextureRequestQueue m_texture_request_queue{};
    StreamingMaterialRequestQueue m_material_request_queue{};

    void consume_create_delta(const RenderableRegistryDelta& delta);
    void consume_update_delta(const RenderableRegistryDelta& delta);
    void consume_destroy_delta(const RenderableRegistryDelta& delta);
};

} // namespace Mizu