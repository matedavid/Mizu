#include "dx12_synchronization.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "dx12_context.h"

namespace Mizu::Dx12
{

//
// Dx12Fence
//

Dx12Fence::Dx12Fence(uint64_t initial_value)
{
    DX12_CHECK(
        Dx12Context.device->handle()->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_handle)));
}

Dx12Fence::~Dx12Fence()
{
    m_handle->Release();
}

uint64_t Dx12Fence::get_completed_value() const
{
    return m_handle->GetCompletedValue();
}

void Dx12Fence::wait(uint64_t value) const
{
    MIZU_PROFILE_SCOPED;

    if (m_handle->GetCompletedValue() >= value)
        return;

    // A null event makes the call block until the value is reached, which avoids sharing an event between threads
    DX12_CHECK(m_handle->SetEventOnCompletion(value, nullptr));
}

void Dx12Fence::signal(uint64_t value)
{
    DX12_CHECK(m_handle->Signal(value));
}

} // namespace Mizu::Dx12