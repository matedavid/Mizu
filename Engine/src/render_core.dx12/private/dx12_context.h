#pragma once

#include <memory>

#include "dx12_core.h"
#include "dx12_descriptors2.h"
#include "dx12_device.h"
#include "dx12_device_memory_allocator.h"
#include "dx12_root_signature.h"

namespace Mizu::Dx12
{

struct Dx12ContextT
{
    ~Dx12ContextT();

    IDXGIFactory4* factory;

#if MIZU_DX12_VALIDATIONS_ENABLED
    ID3D12Debug6* debug_controller;
    ID3D12DebugDevice* debug_device;
    ID3D12InfoQueue1* debug_info_queue;
#endif

    Dx12Device* device;
    std::unique_ptr<Dx12BaseDeviceMemoryAllocator> default_device_allocator;

    std::unique_ptr<Dx12DescriptorManager> descriptor_manager;
    std::unique_ptr<Dx12FreeListCpuDescriptorHeap> rtv_heap;
    std::unique_ptr<Dx12FreeListCpuDescriptorHeap> dsv_heap;
    std::unique_ptr<Dx12FreeListCpuDescriptorHeap> sampler_heap;
    std::unique_ptr<Dx12DescriptorSetLayoutCache> descriptor_set_layout_cache;
    std::unique_ptr<Dx12PipelineLayoutCache> pipeline_layout_cache;

    uint32_t frames_in_flight = 1;
    uint32_t current_frame_idx = 0;
};

extern Dx12ContextT Dx12Context;

} // namespace Mizu::Dx12