#pragma once

#include <memory>

#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"

namespace Mizu
{

class Fence
{
  public:
    virtual ~Fence() = default;

    virtual void wait_for() = 0;
};

class Semaphore
{
  public:
    virtual ~Semaphore() = default;
};

using PipelineStageBitsType = uint32_t;

// clang-format off
enum class PipelineStageBits : PipelineStageBitsType
{
    None = 0,
    VertexInput           = (1 << 0),
    VertexShader          = (1 << 1),
    FragmentShader        = (1 << 2),
    ColorAttachmentOutput = (1 << 3),
    ComputeShader         = (1 << 4),
    Transfer              = (1 << 5),
    AllCommands           = (1 << 6),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(PipelineStageBits, PipelineStageBitsType);

struct WaitSemaphore
{
    std::shared_ptr<Semaphore> semaphore;
    PipelineStageBits stage = PipelineStageBits::AllCommands;
};

struct SignalSemaphore
{
    std::shared_ptr<Semaphore> semaphore;
    PipelineStageBits stage = PipelineStageBits::AllCommands;
};

} // namespace Mizu
