#include "dx12_synchronization.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "dx12_context.h"

namespace Mizu::Dx12
{

//
// Dx12Fence
//

Dx12Fence::Dx12Fence([[maybe_unused]] bool signaled)
{
    DX12_CHECK(Dx12Context.device->handle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_handle)));

    m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    MIZU_ASSERT(m_event != nullptr, "Failed to create event");

    m_next_value = 1;
    m_last_signaled_value = 0;
}

Dx12Fence::~Dx12Fence()
{
    CloseHandle(m_event);
    m_handle->Release();
}

void Dx12Fence::wait_for()
{
    MIZU_PROFILE_SCOPED;

    const uint64_t completed = m_handle->GetCompletedValue();
    if (completed < m_last_signaled_value)
    {
        DX12_CHECK(m_handle->SetEventOnCompletion(m_last_signaled_value, m_event));
        WaitForSingleObject(m_event, INFINITE);
    }
}

void Dx12Fence::signal(ID3D12CommandQueue* queue)
{
    const uint64_t value = m_next_value++;
    DX12_CHECK(queue->Signal(m_handle, value));
    m_last_signaled_value = value;
}

//
// Dx12Semaphore
//

Dx12Semaphore::Dx12Semaphore() : m_counter(0)
{
    DX12_CHECK(Dx12Context.device->handle()->CreateFence(m_counter, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_handle)));
}

Dx12Semaphore::~Dx12Semaphore()
{
    m_handle->Release();
}

void Dx12Semaphore::signal(ID3D12CommandQueue* queue)
{
    m_counter += 1;
    DX12_CHECK(queue->Signal(m_handle, m_counter));
}

void Dx12Semaphore::wait(ID3D12CommandQueue* queue)
{
    DX12_CHECK(queue->Wait(m_handle, m_counter));
}

} // namespace Mizu::Dx12