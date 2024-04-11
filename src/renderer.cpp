#include "renderer.h"

#include <memory>

#include "utility/logging.h"

#include "backend/vulkan/vulkan_backend.h"

namespace Mizu {

static std::unique_ptr<IBackend> s_backend = nullptr;
static Configuration s_config = {};

static void sanity_checks(const Configuration& config) {
    if (!config.requirements.graphics && !config.requirements.compute) {
        MIZU_LOG_WARNING("Neither Graphics nor Compute capabitlies requested, this will result in almost no "
                         "functionality being available");
    }
}

bool initialize(Configuration config) {
    MIZU_LOG_SETUP;

    s_config = std::move(config);
    sanity_checks(s_config);

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
