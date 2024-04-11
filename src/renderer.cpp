#include "renderer.h"

#include <memory>

#include "backend/vulkan/vulkan_backend.h"

namespace Mizu {

static std::unique_ptr<IBackend> s_backend = nullptr;
static Configuration s_config = {};

bool initialize(Configuration config) {
    s_config = std::move(config);

    switch (s_config.graphics_api) {
    default:
    case GraphicsAPI::Vulkan:
        s_backend = std::make_unique<Vulkan::VulkanBackend>();
        break;
    case GraphicsAPI::OpenGL:
        // s_backend = std::make_unique<OpenGL::OpenGLBackend>();
        return false;
        break;
    }

    return s_backend->initialize(s_config);
}

void shutdown() {
    s_backend = nullptr;
    s_config = {};
}

// Configuration get_config() {
//     return m_config;
// }

} // namespace Mizu
