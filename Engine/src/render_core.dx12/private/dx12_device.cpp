#include "dx12_device.h"

#include "base/debug/logging.h"

#include "dx12_buffer_resource.h"
#include "dx12_command_buffer.h"
#include "dx12_context.h"
#include "dx12_descriptors.h"
#include "dx12_framebuffer.h"
#include "dx12_image_resource.h"
#include "dx12_pipeline.h"
#include "dx12_resource_group.h"
#include "dx12_root_signature.h"
#include "dx12_sampler_state.h"
#include "dx12_shader.h"
#include "dx12_swapchain.h"
#include "dx12_synchronization.h"
#include "mizu_render_core_dx12_module.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

static void d3d12_validation_message_callback(
    [[maybe_unused]] D3D12_MESSAGE_CATEGORY category,
    D3D12_MESSAGE_SEVERITY severity,
    [[maybe_unused]] D3D12_MESSAGE_ID id,
    [[maybe_unused]] LPCSTR pDescription,
    [[maybe_unused]] void* pContext)
{
    switch (severity)
    {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
    case D3D12_MESSAGE_SEVERITY_ERROR:
        MIZU_LOG_ERROR("[D3D12 Validation]: {}", pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_WARNING:
        MIZU_LOG_WARNING("[D3D12 Validation]: {}", pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_INFO:
    case D3D12_MESSAGE_SEVERITY_MESSAGE:
        MIZU_LOG_INFO("[D3D12 Validation]: {}", pDescription);
        break;
    }
}

#endif

Dx12Device::Dx12Device(const DeviceCreationDescription& desc)
{
    (void)desc;
    MIZU_ASSERT(
        std::holds_alternative<Dx12SpecificConfiguration>(desc.specific_config),
        "specific_config is not Dx12SpecificConfiguration");

    // Create Factory
    uint32_t dxgi_factory_flags = 0;

#if MIZU_DX12_VALIDATIONS_ENABLED
    ID3D12Debug* debug_controller;
    DX12_CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)));

    DX12_CHECK(debug_controller->QueryInterface(IID_PPV_ARGS(&Dx12Context.debug_controller)));
    Dx12Context.debug_controller->EnableDebugLayer();
    Dx12Context.debug_controller->SetEnableGPUBasedValidation(true);
    Dx12Context.debug_controller->SetEnableSynchronizedCommandQueueValidation(true);

    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;

    debug_controller->Release();
#endif

    DX12_CHECK(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&Dx12Context.factory)));

    // Select adapter
    DX12_CHECK(DXCoreCreateAdapterFactory(&m_factory));

    constexpr uint32_t DISCRETE_GPU_SCORE = 40;

    IDXGIAdapter1* best_adapter = nullptr;
    uint32_t best_adapter_score = 0;

    IDXGIAdapter1* tmp_adapter;
    for (uint32_t i = 0; Dx12Context.factory->EnumAdapters1(i, &tmp_adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 tmp_adapter_desc;
        tmp_adapter->GetDesc1(&tmp_adapter_desc);

        if (tmp_adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select software adapter
            tmp_adapter->Release();
            continue;
        }

        // Check to see if the adapter supports Direct3D 12
        if (!DX12_CHECK_RESULT(D3D12CreateDevice(tmp_adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }

        uint32_t adapter_score = 0;

        IDXCoreAdapter* core_adapter;
        DX12_CHECK(m_factory->GetAdapterByLuid(tmp_adapter_desc.AdapterLuid, &core_adapter));

        bool is_dedicated_gpu = false;
        DX12_CHECK(core_adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &is_dedicated_gpu));

        if (is_dedicated_gpu)
        {
            adapter_score += DISCRETE_GPU_SCORE;
        }

        if (adapter_score > best_adapter_score)
        {
            if (best_adapter != nullptr)
                best_adapter->Release();

            best_adapter_score = adapter_score;
            best_adapter = tmp_adapter;
        }
        else
        {
            tmp_adapter->Release();
        }

        core_adapter->Release();
    }

    MIZU_VERIFY(best_adapter != nullptr, "Failed to find suitable adapter");

    // Create Device
    m_adapter = best_adapter;
    DX12_CHECK_RESULT(D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));

    Dx12Context.device = this;

#if MIZU_DX12_VALIDATIONS_ENABLED
    DX12_CHECK(Dx12Context.device->handle()->QueryInterface(&Dx12Context.debug_device));

    Dx12Context.device->handle()->QueryInterface(IID_PPV_ARGS(&Dx12Context.debug_info_queue));
    DWORD cookie = 0;
    DX12_CHECK(Dx12Context.debug_info_queue->RegisterMessageCallback(
        d3d12_validation_message_callback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &cookie));
#endif

    Dx12Context.heaps.cbv_srv_uav_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        1000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    Dx12Context.heaps.rtv_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        100, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    Dx12Context.heaps.dsv_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        100, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    Dx12Context.heaps.sampler_heap = std::make_unique<Dx12DescriptorHeapCircularBuffer>(
        100, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    Dx12Context.heaps.cbv_srv_uav_shader_heap =
        std::make_unique<Dx12DescriptorHeapGpuCircularBuffer>(100000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Dx12Context.heaps.sampler_shader_heap =
        std::make_unique<Dx12DescriptorHeapGpuCircularBuffer>(1000, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    Dx12Context.root_signature_cache = std::make_unique<Dx12RootSignatureCache>();

    Dx12Context.default_device_allocator = std::make_unique<Dx12BaseDeviceMemoryAllocator>();

    const uint32_t num_transient_persistent_descriptors = 500 + 150 + 100 + 100 + 50;

    Dx12DescriptorManagerDescription descriptor_manager_desc{};
    descriptor_manager_desc.num_transient_descriptors = num_transient_persistent_descriptors;
    descriptor_manager_desc.num_persistent_descriptors = num_transient_persistent_descriptors;
    descriptor_manager_desc.num_bindless_descriptors = 500'000;
    Dx12Context.descriptor_manager = std::make_unique<Dx12DescriptorManager>(descriptor_manager_desc);

    create_queues();
    create_command_allocators();
    retrieve_device_capabilities();
}

Dx12Device::~Dx12Device()
{
    // NOTE: Order of destruction matters

    Dx12Context.descriptor_manager.reset();

    Dx12Context.default_device_allocator.reset();
    Dx12Context.root_signature_cache.reset();

    Dx12Context.heaps.sampler_shader_heap.reset();
    Dx12Context.heaps.cbv_srv_uav_shader_heap.reset();

    Dx12Context.heaps.sampler_heap.reset();
    Dx12Context.heaps.dsv_heap.reset();
    Dx12Context.heaps.rtv_heap.reset();
    Dx12Context.heaps.cbv_srv_uav_heap.reset();

    if (m_transfer_queue != m_graphics_queue)
        m_transfer_queue->Release();

    if (m_compute_queue != m_graphics_queue)
        m_compute_queue->Release();

    m_graphics_queue->Release();

    if (m_transfer_command_allocator != m_graphics_command_allocator)
        m_transfer_command_allocator->Release();

    if (m_compute_command_allocator != m_graphics_command_allocator)
        m_compute_command_allocator->Release();

    m_graphics_command_allocator->Release();

    m_device->Release();
    m_adapter->Release();
    m_factory->Release();

#if MIZU_DX12_VALIDATIONS_ENABLED
    Dx12Context.debug_info_queue->Release();
    Dx12Context.debug_controller->Release();
    Dx12Context.debug_device->Release();
#endif

    Dx12Context.factory->Release();

#if MIZU_DX12_VALIDATIONS_ENABLED
    IDXGIDebug1* dxgi_debug;
    if (DX12_CHECK_RESULT(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug))))
    {
        dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
    }

    dxgi_debug->Release();
#endif
}

void Dx12Device::wait_idle() const
{
    ID3D12Fence* fence;
    DX12_CHECK(Dx12Context.device->handle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    constexpr uint64_t FENCE_VALUE = 1;
    DX12_CHECK(Dx12Context.device->get_graphics_queue()->Signal(fence, FENCE_VALUE));

    const HANDLE event_handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (fence->GetCompletedValue() < FENCE_VALUE)
    {
        DX12_CHECK(fence->SetEventOnCompletion(FENCE_VALUE, event_handle));
        WaitForSingleObject(event_handle, INFINITE);
    }

    CloseHandle(event_handle);
    fence->Release();
}

ID3D12GraphicsCommandList7* Dx12Device::allocate_command_list(CommandBufferType type)
{
    ID3D12CommandAllocator* allocator = get_command_allocator(type);
    D3D12_COMMAND_LIST_TYPE allocator_type = D3D12_COMMAND_LIST_TYPE_NONE;

    switch (type)
    {
    case CommandBufferType::Graphics:
        allocator_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        break;
    case CommandBufferType::Compute:
        allocator_type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        break;
    case CommandBufferType::Transfer:
        allocator_type = D3D12_COMMAND_LIST_TYPE_COPY;
        break;
    }

    MIZU_ASSERT(
        allocator != nullptr && allocator_type != D3D12_COMMAND_LIST_TYPE_NONE, "No command allocator was selected");

    ID3D12GraphicsCommandList7* command_list;
    DX12_CHECK(m_device->CreateCommandList(0, allocator_type, allocator, nullptr, IID_PPV_ARGS(&command_list)));
    command_list->Close();

    return command_list;
}

void Dx12Device::free_command_list(ID3D12GraphicsCommandList7* command_list, [[maybe_unused]] CommandBufferType type)
{
    MIZU_ASSERT(command_list != nullptr, "command_list can't be nullptr");
    command_list->Release();
}

ID3D12CommandAllocator* Dx12Device::get_command_allocator(CommandBufferType type) const
{
    switch (type)
    {
    case CommandBufferType::Graphics:
        return m_graphics_command_allocator;
    case CommandBufferType::Compute:
        return m_compute_command_allocator;
    case CommandBufferType::Transfer:
        return m_transfer_command_allocator;
    }
}

void Dx12Device::create_queues()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    // Graphics queue
    {
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        DX12_CHECK(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_graphics_queue)));

        m_graphics_queue->SetName(L"GraphicsQueue");
    }

    // Compute queue
    {
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

        if (!DX12_CHECK_RESULT(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_compute_queue))))
        {
            m_compute_queue = m_graphics_queue;
        }
        else
        {
            m_compute_queue->SetName(L"ComputeQueue");
        }
    }

    // Transfer queue
    {
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

        if (!DX12_CHECK_RESULT(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_transfer_queue))))
        {
            m_transfer_queue = m_graphics_queue;
        }
        else
        {
            m_transfer_queue->SetName(L"TransferQueue");
        }
    }
}

void Dx12Device::create_command_allocators()
{
    // Graphics allocator
    DX12_CHECK(
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_graphics_command_allocator)));

    // Compute allocator
    if (m_compute_queue != m_graphics_queue)
    {
        DX12_CHECK(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_compute_command_allocator)));
    }
    else
    {
        m_compute_command_allocator = m_graphics_command_allocator;
    }

    // Transfer allocator
    if (m_transfer_queue != m_graphics_queue)
    {
        DX12_CHECK(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_transfer_command_allocator)));
    }
    else
    {
        m_transfer_command_allocator = m_graphics_command_allocator;
    }
}

void Dx12Device::retrieve_device_capabilities()
{
    DXGI_ADAPTER_DESC1 adapter_desc;
    DX12_CHECK(m_adapter->GetDesc1(&adapter_desc));

    std::wstring wname(adapter_desc.Description);
    const int32_t size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);

    std::string name(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &name[0], size_needed, nullptr, nullptr);

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    DX12_CHECK(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

    m_properties = {};
    m_properties.name = name;
    m_properties.ray_tracing_hardware = options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    m_properties.async_compute = m_compute_queue != m_graphics_queue;
}

//
// Creation functions
//

std::shared_ptr<BufferResource> Dx12Device::create_buffer(const BufferDescription& desc) const
{
    return std::make_shared<Dx12BufferResource>(desc);
}

std::shared_ptr<ImageResource> Dx12Device::create_image(const ImageDescription& desc) const
{
    return std::make_shared<Dx12ImageResource>(desc);
}

std::shared_ptr<AccelerationStructure> Dx12Device::create_acceleration_structure(
    const AccelerationStructureDescription& desc) const
{
    (void)desc;
    MIZU_UNREACHABLE("Not implemented");
    return nullptr;
}

std::shared_ptr<CommandBuffer> Dx12Device::create_command_buffer(CommandBufferType type) const
{
    return std::make_shared<Dx12CommandBuffer>(type);
}

std::shared_ptr<Framebuffer> Dx12Device::create_framebuffer(const FramebufferDescription& desc) const
{
    return std::make_shared<Dx12Framebuffer>(desc);
}

std::shared_ptr<Shader> Dx12Device::create_shader(const ShaderDescription& desc) const
{
    return std::make_shared<Dx12Shader>(desc);
}

std::shared_ptr<SamplerState> Dx12Device::create_sampler_state(const SamplerStateDescription& desc) const
{
    return std::make_shared<Dx12SamplerState>(desc);
}

std::shared_ptr<Pipeline> Dx12Device::create_pipeline(const GraphicsPipelineDescription& desc) const
{
    return std::make_shared<Dx12Pipeline>(desc);
}

std::shared_ptr<Pipeline> Dx12Device::create_pipeline(const ComputePipelineDescription& desc) const
{
    return std::make_shared<Dx12Pipeline>(desc);
}

std::shared_ptr<Pipeline> Dx12Device::create_pipeline(const RayTracingPipelineDescription& desc) const
{
    return std::make_shared<Dx12Pipeline>(desc);
}

std::shared_ptr<ResourceGroup> Dx12Device::create_resource_group(const ResourceGroupBuilder& builder) const
{
    return std::make_shared<Dx12ResourceGroup>(builder);
}

std::shared_ptr<DescriptorSet> Dx12Device::allocate_descriptor_set(
    std::span<const DescriptorItem> layout,
    DescriptorSetAllocationType type) const
{
    switch (type)
    {
    case DescriptorSetAllocationType::Transient:
        return Dx12Context.descriptor_manager->allocate_transient(layout);
    case DescriptorSetAllocationType::Persistent:
        return Dx12Context.descriptor_manager->allocate_persistent(layout);
    case DescriptorSetAllocationType::Bindless:
        return Dx12Context.descriptor_manager->allocate_bindless(layout);
    }
}

void Dx12Device::reset_transient_descriptors()
{
    Dx12Context.descriptor_manager->reset_transient();
}

std::shared_ptr<Semaphore> Dx12Device::create_semaphore() const
{
    return std::make_shared<Dx12Semaphore>();
}

std::shared_ptr<Fence> Dx12Device::create_fence(bool signaled) const
{
    return std::make_shared<Dx12Fence>(signaled);
}

std::shared_ptr<Swapchain> Dx12Device::create_swapchain(const SwapchainDescription& desc) const
{
    return std::make_shared<Dx12Swapchain>(desc);
}

std::shared_ptr<AliasedDeviceMemoryAllocator> Dx12Device::create_aliased_memory_allocator(
    bool host_visible,
    std::string name) const
{
    return std::make_shared<Dx12AliasedDeviceMemoryAllocator>(host_visible, name);
}

} // namespace Mizu::Dx12

extern "C" MIZU_RENDER_CORE_DX12_API Mizu::Device* create_rhi_device(const Mizu::DeviceCreationDescription& desc)
{
    return new Mizu::Dx12::Dx12Device{desc};
}