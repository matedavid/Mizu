#pragma once

#include <span>
#include <vector>

#include "asset/asset_handle.h"

namespace Mizu
{

class CommandBuffer;
class IStreamingManager;
struct RenderViewRegistryEntry;

struct StreamingContext
{
    uint64_t frame_num;
    std::span<const RenderViewRegistryEntry> views;
    glm::uvec2 viewport_size;
};

struct StreamingRequest
{
    IStreamingManager* manager = nullptr;
    AssetKey asset_key = {};

    uint8_t resident_level = 0;
    uint8_t desired_level = 0;
    int8_t level_delta = 0;

    float score = 0.0f;
};

struct StreamingAward
{
};

class IStreamingManager
{
  public:
    virtual ~IStreamingManager() = default;

    virtual void update(const StreamingContext& context) = 0;
    virtual void gather(std::vector<StreamingRequest>& requests) = 0;
    virtual void execute(std::span<StreamingAward> awards) = 0;

    virtual void record_gpu_uploads(CommandBuffer& command) = 0;
};

template <typename T>
concept StreamingManagerType = requires() { std::is_base_of_v<IStreamingManager, T>; };

} // namespace Mizu
