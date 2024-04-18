#pragma once

#include <memory>
#include <string>

namespace Mizu {

// Forward declarations
class ICommandBuffer;
class VertexBuffer;
class IndexBuffer;

enum class GraphicsAPI {
    Vulkan,
    // OpenGL,
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

    virtual void draw(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const = 0;
    virtual void draw_indexed(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const = 0;
};

bool initialize(Configuration config);
void shutdown();

void draw(const std::shared_ptr<ICommandBuffer>& command_buffer, const std::shared_ptr<VertexBuffer>& vertex);
void draw_indexed(const std::shared_ptr<ICommandBuffer>& command_buffer,
                  const std::shared_ptr<VertexBuffer>& vertex,
                  const std::shared_ptr<IndexBuffer>& index);

Configuration get_config();

} // namespace Mizu