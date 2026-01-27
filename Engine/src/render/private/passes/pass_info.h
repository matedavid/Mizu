#pragma once

#include <cstdint>

#include "render/render_graph/render_graph_types.h"

namespace Mizu
{

struct FrameInfo
{
    uint32_t width, height;
    uint32_t frame_idx;
    RGImageRef output_texture_ref;
};

} // namespace Mizu
