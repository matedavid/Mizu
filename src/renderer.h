#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Mizu {

// Forward declarations
class ICommandBuffer;
class VertexBuffer;
class IndexBuffer;

enum class GraphicsAPI {
    Vulkan,
    OpenGL,
};

struct Version {
    uint32_t major = 0;
    uint32_t minor = 1;
    uint32_t patch = 0;
};

struct Requirements {
    bool graphics = true;
    bool compute = true;
};

struct VulkanSpecificConfiguration {
    std::vector<std::string> instance_extensions{};
};

struct OpenGLSpecificConfiguration {
    bool create_context = false;
};

using BackendSpecificConfiguration = std::variant<VulkanSpecificConfiguration, OpenGLSpecificConfiguration>;

struct Configuration {
    GraphicsAPI graphics_api = GraphicsAPI::Vulkan;
    BackendSpecificConfiguration backend_specific_config = VulkanSpecificConfiguration{};

    Requirements requirements{};

    std::string application_name{};
    Version application_version{};
};

class IBackend {
  public:
    virtual ~IBackend() = default;

    virtual bool initialize(const Configuration& config) = 0;
};

bool initialize(Configuration config);
void shutdown();

Configuration get_config();

} // namespace Mizu