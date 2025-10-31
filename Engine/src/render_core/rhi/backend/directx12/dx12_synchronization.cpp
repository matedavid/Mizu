#include "dx12_synchronization.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"

namespace Mizu::Dx12
{

//
// Dx12Fence
//

Dx12Fence::Dx12Fence(bool signaled) : m_counter(0), m_signal_registered(signaled), m_first_wait_for(signaled)
{
    m_counter = signaled ? 0 : 1;
    DX12_CHECK(Dx12Context.device->handle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_handle)));

    m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    MIZU_ASSERT(m_event != nullptr, "Failed to create event");
}

Dx12Fence::~Dx12Fence()
{
    CloseHandle(m_event);
    m_handle->Release();
}

void Dx12Fence::wait_for()
{
    MIZU_PROFILE_SCOPED;

    if (m_first_wait_for)
    {
        // HACK: Path to make d3d12 fence match vulkans behaviour. If is signaled by default, the first
        // wait_for should not wait and unsignal the fence (as in vulkan we call vkResetFence). Therefore we early
        // return and set the signal registered to false. In the case where the fence is not signaled by default, we
        // should wait for the signal being registered, so this path should not be taken.

        m_first_wait_for = false;
        m_signal_registered = false;
        return;
    }

    MIZU_ASSERT(m_signal_registered, "Trying to wait for a fence that has no signals registered");

    if (m_handle->GetCompletedValue() < m_counter)
    {
        m_handle->SetEventOnCompletion(m_counter, m_event);
        WaitForSingleObject(m_event, INFINITE);
    }

    m_signal_registered = false;
}

void Dx12Fence::signal(ID3D12CommandQueue* queue)
{
    MIZU_ASSERT(!m_signal_registered, "Can't signal a fence that is already signaled");

    m_signal_registered = true;
    m_counter += 1;
    DX12_CHECK(queue->Signal(m_handle, m_counter));
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