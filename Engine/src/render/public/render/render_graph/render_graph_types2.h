#pragma once

#include <cstdint>
#include <limits>

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

struct RenderGraphExternalBufferState
{
    BufferResourceState initial_state = BufferResourceState::ShaderReadOnly;
    BufferResourceState final_state = BufferResourceState::ShaderReadOnly;
};

struct RenderGraphExternalImageState
{
    ImageResourceState initial_state = ImageResourceState::ShaderReadOnly;
    ImageResourceState final_state = ImageResourceState::ShaderReadOnly;
};

constexpr uint64_t INVALID_RENDER_GRAPH_RESOURCE = std::numeric_limits<uint64_t>::max();

struct RenderGraphResource
{
    uint64_t id = INVALID_RENDER_GRAPH_RESOURCE;

    bool operator==(const RenderGraphResource& other) const { return id == other.id; }
};

} // namespace Mizu

template <>
struct std::hash<Mizu::RenderGraphResource>
{
    size_t operator()(const Mizu::RenderGraphResource& resource) const { return resource.id; }
};
