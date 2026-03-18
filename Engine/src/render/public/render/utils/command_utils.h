#pragma once

#include "render_core/rhi/command_buffer.h"

#include "mizu_render_module.h"

namespace Mizu
{
namespace CommandUtils
{

using SubmitSingleTimeFunc = std::function<void(CommandBuffer&)>;
MIZU_RENDER_API void submit_single_time(CommandBufferType type, const SubmitSingleTimeFunc& func);

} // namespace CommandUtils

} // namespace Mizu