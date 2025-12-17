#include "backend/dx12/dx12_device.h"

#include "base/debug/logging.h"

#include "backend/dx12/dx12_context.h"

namespace Mizu::Dx12
{

Dx12Device::Dx12Device()
{
    DX12_CHECK(DXCoreCreateAdapterFactory(&m_factory));

    constexpr uint32_t DISCRETE_GPU_SCORE = 40;

    IDXGIAdapter1* best_adapter = nullptr;
    uint32_t best_adapter_score = 0;

    IDXGIAdapter1* tmp_adapter;
    for (uint32_t i = 0; Dx12Context.factory->EnumAdapters1(i, &tmp_adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        tmp_adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
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
        DX12_CHECK(m_factory->GetAdapterByLuid(desc.AdapterLuid, &core_adapter));

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

    m_adapter = best_adapter;
    DX12_CHECK_RESULT(D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));

#if MIZU_DEBUG
    DXGI_ADAPTER_DESC1 adapter_desc;
    m_adapter->GetDesc1(&adapter_desc);

    std::wstring wname(adapter_desc.Description);
    const int32_t size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);

    std::string name(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &name[0], size_needed, nullptr, nullptr);

    MIZU_LOG_INFO("Selected device: {}", name);
#endif

    create_queues();
    create_command_allocators();
    retrieve_device_capabilities();
}

Dx12Device::~Dx12Device()
{
    {
        if (m_transfer_queue != m_graphics_queue)
            m_transfer_queue->Release();

        if (m_compute_queue != m_graphics_queue)
            m_compute_queue->Release();

        m_graphics_queue->Release();
    }

    {
        if (m_transfer_command_allocator != m_graphics_command_allocator)
            m_transfer_command_allocator->Release();

        if (m_compute_command_allocator != m_graphics_command_allocator)
            m_compute_command_allocator->Release();

        m_graphics_command_allocator->Release();
    }

    m_device->Release();
    m_adapter->Release();
    m_factory->Release();
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
    m_capabilities = {};

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    DX12_CHECK(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

    // TODO: TEMPORAL, should remove this capability as this does not make sense in d3d12
    m_capabilities.max_resource_group_sets = 10;

    m_capabilities.ray_tracing_hardware = options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    m_capabilities.async_compute = m_compute_queue != m_graphics_queue;
}

} // namespace Mizu::Dx12