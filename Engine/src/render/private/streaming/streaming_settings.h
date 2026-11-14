#pragma once

#include "core/settings_manager/settings_manager.h"
#include "render_core/rhi/device.h"

namespace Mizu
{

enum class StreamingMode
{
    Enabled,
    FullResidency,
    Frozen,
};

#define MIZU_STREAMING_SETTINGS_MEMBERS(X)         \
    X(StreamingMode, mode, StreamingMode::Enabled) \
    X(bool, parallel_update, true)

MIZU_CREATE_SETTING(StreamingSettings, MIZU_STREAMING_SETTINGS_MEMBERS);

#undef MIZU_STREAMING_SETTINGS_MEMBERS

} // namespace Mizu