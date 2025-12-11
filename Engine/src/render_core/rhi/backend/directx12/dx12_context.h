#pragma once

#include <memory>
#include <string_view>

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/backend/directx12/dx12_descriptors.h"
#include "render_core/rhi/backend/directx12/dx12_device.h"
#include "render_core/rhi/backend/directx12/dx12_root_signature.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

class Dx12Debug
{
  public:
    static void set_resource_name(ID3D12Resource* resource, std::string_view name);
};

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name) Dx12Debug::set_resource_name(resource, name)

#else

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name)

#endif

class Dx12DescriptorHeapCircularBuffer;

struct DescriptorHeaps
{
    std::unique_ptr<Dx12DescriptorHeapCircularBuffer> cbv_srv_uav_heap;
    std::unique_ptr<Dx12DescriptorHeapCircularBuffer> rtv_heap;
    std::unique_ptr<Dx12DescriptorHeapCircularBuffer> dsv_heap;
    std::unique_ptr<Dx12DescriptorHeapCircularBuffer> sampler_heap;

    std::unique_ptr<Dx12DescriptorHeapGpuCircularBuffer> cbv_srv_uav_shader_heap;
    std::unique_ptr<Dx12DescriptorHeapGpuCircularBuffer> sampler_shader_heap;
};

struct Dx12ContextT
{
    ~Dx12ContextT();

    IDXGIFactory4* factory;

#if MIZU_DX12_VALIDATIONS_ENABLED
    ID3D12Debug6* debug_controller;
    ID3D12DebugDevice* debug_device;
    ID3D12InfoQueue1* debug_info_queue;
#endif

    std::unique_ptr<Dx12Device> device;
    DescriptorHeaps heaps;
    std::unique_ptr<Dx12RootSignatureCache> root_signature_cache;
};

extern Dx12ContextT Dx12Context;

} // namespace Mizu::Dx12