#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/renderer.h"

namespace Mizu::Dx12
{

class Dx12Device
{
  public:
    Dx12Device();
    ~Dx12Device();

    ID3D12CommandQueue* get_graphics_queue() const { return m_graphics_queue; }
    ID3D12CommandQueue* get_compute_queue() const { return m_compute_queue; }
    ID3D12CommandQueue* get_transfer_queue() const { return m_transfer_queue; }

    RendererCapabilities get_device_capabilities() const { return m_capabilities; }

    ID3D12Device* handle() const { return m_device; }

  private:
    ID3D12Device* m_device = nullptr;
    IDXGIAdapter1* m_adapter = nullptr;

    ID3D12CommandQueue* m_graphics_queue = nullptr;
    ID3D12CommandQueue* m_compute_queue = nullptr;
    ID3D12CommandQueue* m_transfer_queue = nullptr;

    RendererCapabilities m_capabilities{};

    void create_queues();
    void retrieve_device_capabilities();
};

} // namespace Mizu::Dx12