#pragma once

#include "render_core/rhi/command_buffer.h"

namespace Mizu
{
namespace CommandUtils
{

using SubmitSingleTimeFunc = std::function<void(CommandBuffer&)>;
void submit_single_time(CommandBufferType type, const SubmitSingleTimeFunc& func);

} // namespace CommandUtils

} // namespace Mizu