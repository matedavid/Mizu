#pragma once

#include <string>
#include <variant>
#include <vector>

#include "renderer/abstraction/device_memory_allocator.h"

namespace Mizu
{

// Forward declarations
class ICommandBuffer;
class VertexBuffer;
class IndexBuffer;

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

struct Requirements
{
    bool graphics = true;
    bool compute = true;
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

    Requirements requirements{};

    std::string application_name{};
    Version application_version{};
};

class IBackend
{
  public:
    virtual ~IBackend() = default;

    virtual bool initialize(const RendererConfiguration& config) = 0;

    virtual void wait_idle() const = 0;
};

class Renderer
{
  public:
    static bool initialize(RendererConfiguration config);
    static void shutdown();

    static void wait_idle();

    static std::shared_ptr<IDeviceMemoryAllocator> get_allocator();

    static RendererConfiguration get_config();
};

} // namespace Mizu
