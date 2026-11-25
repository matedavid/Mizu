#pragma once

#include "core/settings_manager/settings_manager.h"

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
    X(bool, parallel_update, true)                 \
    X(bool, logging_enabled, false)                \
    X(bool, sparse_enabled, false) // TODO: Temporary false until discover why sparse is slow

MIZU_CREATE_SETTING(StreamingSettings, MIZU_STREAMING_SETTINGS_MEMBERS);

#undef MIZU_STREAMING_SETTINGS_MEMBERS

} // namespace Mizu