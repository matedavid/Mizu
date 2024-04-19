#include "renderer.h"

#include <memory>

#include "utility/logging.h"

#include "buffers.h"
#include "command_buffer.h"

#include "backend/vulkan/vulkan_backend.h"

namespace Mizu {

static std::unique_ptr<IBackend> s_backend = nullptr;
static Configuration s_config = {};

static void sanity_checks(const Configuration& config) {
    // Check: No capability requested
    if (!config.requirements.graphics && !config.requirements.compute) {
        MIZU_LOG_WARNING("Neither Graphics nor Compute capabilities requested, this will result in almost no "
                         "functionality being available");
    }

    // Check: backend_specific_config does not match with graphics_api requested
    switch (config.graphics_api) {
    case GraphicsAPI::Vulkan: {
        if (!std::holds_alternative<VulkanSpecificConfiguration>(config.backend_specific_config)) {
            MIZU_LOG_ERROR("Vulkan API requested but backend_specific_config is not VulkanSpecificConfiguration");
        }
    } break;
    }
}

bool initialize(Configuration config) {
    MIZU_LOG_SETUP;

    s_config = std::move(config);
    sanity_checks(s_config);

    switch (s_config.graphics_api) {
    case GraphicsAPI::Vulkan:
        s_backend = std::make_unique<Vulkan::VulkanBackend>();
        break;
    }

    return s_backend->initialize(s_config);
}

void shutdown() {
    s_backend = nullptr;
    s_config = {};
}

void draw(const std::shared_ptr<ICommandBuffer>& command_buffer, const std::shared_ptr<VertexBuffer>& vertex) {
    vertex->bind(command_buffer);

    s_backend->draw(command_buffer, vertex->count());
}

void draw_indexed(const std::shared_ptr<ICommandBuffer>& command_buffer,
                  const std::shared_ptr<VertexBuffer>& vertex,
                  const std::shared_ptr<IndexBuffer>& index) {
    vertex->bind(command_buffer);
    index->bind(command_buffer);

    s_backend->draw_indexed(command_buffer, index->count());
}

Configuration get_config() {
    return s_config;
}

} // namespace Mizu
