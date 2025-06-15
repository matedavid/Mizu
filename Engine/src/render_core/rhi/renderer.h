#pragma once

#include <string>
#include <variant>
#include <vector>

#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/systems/pipeline_cache.h"
#include "render_core/systems/sampler_state_cache.h"

namespace Mizu
{

enum class GraphicsAPI
{
    Vulkan,
    OpenGL,
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

struct OpenGLSpecificConfiguration
{
    bool create_context = false;
};

using BackendSpecificConfiguration = std::variant<VulkanSpecificConfiguration, OpenGLSpecificConfiguration>;

struct RendererConfiguration
{
    GraphicsAPI graphics_api = GraphicsAPI::Vulkan;
    BackendSpecificConfiguration backend_specific_config = VulkanSpecificConfiguration{};

    std::string application_name{};
    Version application_version{};
};

struct RendererCapabilities
{
    uint32_t max_resource_group_sets;
    uint32_t max_push_constant_size;

    bool ray_tracing_hardware;
};

class IBackend
{
  public:
    virtual ~IBackend() = default;

    virtual bool initialize(const RendererConfiguration& config) = 0;

    virtual void wait_idle() const = 0;

    virtual RendererCapabilities get_capabilities() const = 0;
};

class Renderer
{
  public:
    static bool initialize(RendererConfiguration config);
    static void shutdown();

    static void wait_idle();

    static std::shared_ptr<IDeviceMemoryAllocator> get_allocator();
    static std::shared_ptr<PipelineCache> get_pipeline_cache();
    static std::shared_ptr<SamplerStateCache> get_sampler_state_cache();

    static RendererConfiguration get_config();
    static RendererCapabilities get_capabilities();
};

} // namespace Mizu
