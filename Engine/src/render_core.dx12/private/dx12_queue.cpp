#include "dx12_queue.h"

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "dx12_command_buffer.h"
#include "dx12_context.h"
#include "dx12_synchronization.h"

namespace Mizu::Dx12
{

static ID3D12Fence* get_native_fence(const SyncPoint& point)
{
    return static_cast<const Dx12Fence*>(point.fence)->handle();
}

Dx12Queue::Dx12Queue(ID3D12CommandQueue* queue, CommandBufferType type) : m_queue(queue), m_type(type)
{
    m_fence = std::make_unique<Dx12Fence>(0);
}

Dx12Queue::~Dx12Queue()
{
    m_queue->Release();
}

SyncPoint Dx12Queue::submit(std::span<CommandBuffer*> command_buffers, const SubmitInfo& info)
{
    MIZU_PROFILE_SCOPED;

    constexpr size_t MAX_COMMAND_LISTS = 16;
    MIZU_ASSERT(
        command_buffers.size() <= MAX_COMMAND_LISTS,
        "Too many command buffers in a single submit ({} > {})",
        command_buffers.size(),
        MAX_COMMAND_LISTS);

    inplace_vector<ID3D12CommandList*, MAX_COMMAND_LISTS> command_lists;
    for (const CommandBuffer* command_buffer : command_buffers)
    {
        MIZU_ASSERT(
            &Dx12Context.device->get_native_queue(command_buffer->get_type()) == this,
            "Command buffer can't be submitted to this queue");

        command_lists.push_back(static_cast<const Dx12CommandBuffer*>(command_buffer)->handle());
    }

    const std::lock_guard lock(m_mutex);

    for (const SyncPoint& wait : info.waits)
    {
        if (wait.is_valid())
            DX12_CHECK(m_queue->Wait(get_native_fence(wait), wait.value));
    }

    if (!command_lists.empty())
        m_queue->ExecuteCommandLists(static_cast<UINT>(command_lists.size()), command_lists.data());

    m_last_value += 1;
    DX12_CHECK(m_queue->Signal(m_fence->handle(), m_last_value));

    for (const SyncPoint& signal : info.signals)
        DX12_CHECK(m_queue->Signal(get_native_fence(signal), signal.value));

    return SyncPoint{m_fence.get(), m_last_value};
}

void Dx12Queue::wait(const SyncPoint& point)
{
    if (!point.is_valid())
        return;

    const std::lock_guard lock(m_mutex);
    DX12_CHECK(m_queue->Wait(get_native_fence(point), point.value));
}

SyncPoint Dx12Queue::get_last_submitted() const
{
    const std::lock_guard lock(m_mutex);
    return SyncPoint{m_fence.get(), m_last_value};
}

void Dx12Queue::wait_idle()
{
    MIZU_PROFILE_SCOPED;

    uint64_t value = 0;
    {
        const std::lock_guard lock(m_mutex);

        value = ++m_last_value;
        DX12_CHECK(m_queue->Signal(m_fence->handle(), value));
    }

    m_fence->wait(value);
}

} // namespace Mizu::Dx12
