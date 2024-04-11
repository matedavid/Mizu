#pragma once

#include <string>

namespace Mizu {

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

struct Configuration {
    GraphicsAPI graphics_api = GraphicsAPI::Vulkan;

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

// Configuration get_config();

} // namespace Mizu