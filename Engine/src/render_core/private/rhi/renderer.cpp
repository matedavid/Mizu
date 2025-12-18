#include "render_core/rhi/renderer.h"

#include <memory>

#include "base/debug/logging.h"

#include "backend/dx12/dx12_backend.h"

namespace Mizu
{

#ifndef MIZU_ENGINE_SHADERS_PATH
#define MIZU_ENGINE_SHADERS_PATH ""
#endif

static std::unique_ptr<IBackend> s_backend = nullptr;
static std::shared_ptr<BaseDeviceMemoryAllocator> s_memory_allocator = nullptr;
static RendererConfiguration s_config = {};
static RendererCapabilities s_capabilities = {};

static void sanity_checks(const RendererConfiguration& config)
{
    // Check: backend_specific_config does not match with graphics_api requested
    switch (config.graphics_api)
    {
    case GraphicsApi::DirectX12: {
        if (!std::holds_alternative<Dx12SpecificConfiguration>(config.backend_specific_config))
        {
            MIZU_LOG_ERROR("DirectX12 API requested but backend_specific_config is not Dx12SpecificConfiguration");
        }
        break;
    }
    case GraphicsApi::Vulkan: {
        if (!std::holds_alternative<VulkanSpecificConfiguration>(config.backend_specific_config))
        {
            MIZU_LOG_ERROR("Vulkan API requested but backend_specific_config is not VulkanSpecificConfiguration");
        }
        break;
    }
    }
}

bool Renderer::initialize(RendererConfiguration config)
{
    s_config = std::move(config);
    sanity_checks(s_config);

    switch (s_config.graphics_api)
    {
    case GraphicsApi::DirectX12:
        s_backend = std::make_unique<Dx12::Dx12Backend>();
        break;
    case GraphicsApi::Vulkan:
        s_backend = nullptr;
        break;
    }

    const bool success = s_backend->initialize(s_config);
    if (success)
    {
        s_capabilities = s_backend->get_capabilities();

        MIZU_LOG_INFO("Device capabilities:");
        MIZU_LOG_INFO("    Max resource group sets: {}", s_capabilities.max_resource_group_sets);
        MIZU_LOG_INFO("    Max push constant size:  {}", s_capabilities.max_push_constant_size);
        MIZU_LOG_INFO("    Async compute:           {}", s_capabilities.async_compute);
        MIZU_LOG_INFO("    Ray tracing hardware:    {}", s_capabilities.ray_tracing_hardware);
    }

    s_memory_allocator = BaseDeviceMemoryAllocator::create();

    return success;
}

void Renderer::shutdown()
{
    wait_idle();

    s_memory_allocator = nullptr;
    s_backend = nullptr;
    s_config = {};
    s_capabilities = {};
}

void Renderer::wait_idle()
{
    s_backend->wait_idle();
}

std::shared_ptr<IDeviceMemoryAllocator> Renderer::get_allocator()
{
    return s_memory_allocator;
}

RendererConfiguration Renderer::get_config()
{
    return s_config;
}

RendererCapabilities Renderer::get_capabilities()
{
    return s_capabilities;
}

} // namespace Mizu
