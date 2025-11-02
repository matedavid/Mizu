#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/command_buffer.h"
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

    ID3D12GraphicsCommandList4* allocate_command_list(CommandBufferType type);
    void free_command_list(ID3D12GraphicsCommandList4* command_list, CommandBufferType type);

    ID3D12CommandAllocator* get_command_allocator(CommandBufferType type) const;

    RendererCapabilities get_device_capabilities() const { return m_capabilities; }

    ID3D12Device* handle() const { return m_device; }

  private:
    IDXCoreAdapterFactory* m_factory = nullptr;
    ID3D12Device* m_device = nullptr;
    IDXGIAdapter1* m_adapter = nullptr;

    ID3D12CommandQueue* m_graphics_queue = nullptr;
    ID3D12CommandQueue* m_compute_queue = nullptr;
    ID3D12CommandQueue* m_transfer_queue = nullptr;

    ID3D12CommandAllocator* m_graphics_command_allocator = nullptr;
    ID3D12CommandAllocator* m_compute_command_allocator = nullptr;
    ID3D12CommandAllocator* m_transfer_command_allocator = nullptr;

    RendererCapabilities m_capabilities{};

    void create_queues();
    void create_command_allocators();
    void retrieve_device_capabilities();
};

} // namespace Mizu::Dx12