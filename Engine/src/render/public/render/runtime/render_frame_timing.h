#pragma once

#include <cstdint>

namespace Mizu
{

struct RenderFrameTiming
{
    uint64_t render_time_us = 0;
    double frame_delta_seconds = 0.0;
};

} // namespace Mizu