#include "renderer.h"

#include <memory>
#include <cassert>

#include "backend/vulkan/vulkan_backend.h"

namespace Mizu {

static std::unique_ptr<IBackend> m_backend = nullptr;
static Configuration m_config = {};

bool initialize(Configuration config) {
    m_config = std::move(config);

    switch (m_config.graphics_api) {
    default:
    case GraphicsAPI::Vulkan:
        m_backend = std::make_unique<Vulkan::VulkanBackend>();
        break;
    case GraphicsAPI::OpenGL:
        // m_backend = std::make_unique<OpenGL::OpenGLBackend>();
        return false;
        break;
    }

    return m_backend->initialize(m_config);
}

void shutdown() {
    m_backend = nullptr;
    m_config = {};
}

// Configuration get_config() {
//     return m_config;
// }

} // namespace Mizu
