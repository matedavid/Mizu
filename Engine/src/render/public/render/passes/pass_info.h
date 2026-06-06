#pragma once

#include <cstdint>
#include <memory>

#include "render/render_graph/render_graph_types.h"

namespace Mizu
{

class ImageResource;
class FrameLinearAllocator;

struct FrameInfo
{
    uint32_t width, height;
    uint64_t frame_num;
    double last_frame_time;
    FrameLinearAllocator* frame_allocator;

    std::shared_ptr<ImageResource> output_texture;
    RenderGraphResource output_texture_ref;
};

} // namespace Mizu
