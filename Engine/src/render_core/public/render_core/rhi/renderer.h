#pragma once

#include <string>
#include <variant>
#include <vector>

#include "mizu_render_core_module.h"
#include "render_core/rhi/device_memory_allocator.h"

namespace Mizu
{

enum class GraphicsApi
{
    DirectX12,
    Vulkan,
};

struct Version
{
    uint32_t major = 0;
    uint32_t minor = 1;
    uint32_t patch = 0;
};

struct VulkanSpecificConfiguration
{
    std::vector<const char*> instance_extensions{};
};

struct Dx12SpecificConfiguration
{
};

using BackendSpecificConfiguration = std::variant<VulkanSpecificConfiguration, Dx12SpecificConfiguration>;

struct RendererConfiguration
{
    GraphicsApi graphics_api = GraphicsApi::Vulkan;
    BackendSpecificConfiguration backend_specific_config = VulkanSpecificConfiguration{};

    std::string application_name{};
    Version application_version{};
};

struct RendererCapabilities
{
    uint32_t max_resource_group_sets;
    uint32_t max_push_constant_size;

    bool depth_clamp_enabled;

    bool async_compute;
    bool ray_tracing_hardware;
};

class MIZU_RENDER_CORE_API IBackend
{
  public:
    virtual ~IBackend() = default;

    virtual bool initialize(const RendererConfiguration& config) = 0;

    virtual void wait_idle() const = 0;

    virtual RendererCapabilities get_capabilities() const = 0;
};

class MIZU_RENDER_CORE_API Renderer
{
  public:
    static bool initialize(RendererConfiguration config);
    static void shutdown();

    static void wait_idle();

    static std::shared_ptr<IDeviceMemoryAllocator> get_allocator();

    static RendererConfiguration get_config();
    static RendererCapabilities get_capabilities();
};

} // namespace Mizu
