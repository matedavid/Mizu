#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <span>
#include <vector>

#include "asset/asset_handle.h"

#include "streaming/staging_pool.h"

namespace Mizu
{

class CommandBuffer;
class FrameLinearAllocator;
class RenderGraphBuilder;
class RenderGraphPassBuilder;
struct RenderViewRegistryEntry;
struct StreamingSettings;

struct StreamingFrameData
{
    uint64_t frame_num;
    std::span<const RenderViewRegistryEntry> views;
    glm::uvec2 viewport_size;
};

struct StreamingContext
{
    const StreamingFrameData& frame;
    const StreamingSettings& settings;
};

struct StreamingRequest
{
    uint32_t manager_index = 0;
    AssetKey asset_key = {};

    uint8_t resident_level = 0;
    uint8_t desired_level = 0;
    int8_t level_delta = 0;

    float score = 0.0f;
    uint64_t staging_size_bytes = 0;
};

struct StreamingAward
{
    StreamingRequest request{};
    StagingBlock staging_block{};
};

class IStreamingManager
{
  public:
    virtual ~IStreamingManager() = default;

    virtual void update(const StreamingContext& context) = 0;
    virtual void gather(std::vector<StreamingRequest>& requests) = 0;
    virtual void execute(std::span<StreamingAward> awards) = 0;
    virtual void post_execute() = 0;

    virtual void setup_render_graph_upload_pass(RenderGraphBuilder& /*builder*/, RenderGraphPassBuilder& /*pass*/) {}
    virtual void record_gpu_uploads(CommandBuffer& command, FrameLinearAllocator& frame_allocator) = 0;
};

template <typename T>
concept StreamingManagerType = std::derived_from<T, IStreamingManager>;

} // namespace Mizu
