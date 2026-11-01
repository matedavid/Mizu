#pragma once

#include <span>

#include "base/containers/inplace_vector.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/synchronization.h"

namespace Mizu
{

struct SubmitInfo
{
    static constexpr size_t MAX_WAITS = 8;
    static constexpr size_t MAX_SIGNALS = 4;

    inplace_vector<SyncPoint, MAX_WAITS> waits{};
    inplace_vector<SyncPoint, MAX_SIGNALS> signals{};
};

class Queue
{
  public:
    virtual ~Queue() = default;

    virtual CommandBufferType get_type() const = 0;

    virtual SyncPoint submit(std::span<CommandBuffer*> command_buffers, const SubmitInfo& info = {}) = 0;

    SyncPoint submit(CommandBuffer& command_buffer, const SubmitInfo& info = {})
    {
        CommandBuffer* command_buffer_ptr = &command_buffer;
        return submit(std::span<CommandBuffer*>(&command_buffer_ptr, 1), info);
    }

    virtual void wait(const SyncPoint& point) = 0;

    virtual SyncPoint get_last_submitted() const = 0;
    virtual void wait_idle() = 0;
};

} // namespace Mizu
