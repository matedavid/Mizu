#include "renderer.h"

#include <memory>

#include "managers/shader_manager.h"

#include "render_core/rhi/backend/opengl/opengl_backend.h"
#include "render_core/rhi/backend/vulkan/vulkan_backend.h"

#include "utility/logging.h"

namespace Mizu
{

#ifndef MIZU_ENGINE_SHADERS_PATH
#define MIZU_ENGINE_SHADERS_PATH ""
#endif

static std::unique_ptr<IBackend> s_backend = nullptr;
static std::shared_ptr<BaseDeviceMemoryAllocator> s_memory_allocator = nullptr;
static RendererConfiguration s_config = {};

static void sanity_checks(const RendererConfiguration& config)
{
    // Check: No capability requested
    if (!config.requirements.graphics && !config.requirements.compute)
    {
        MIZU_LOG_WARNING("Neither Graphics nor Compute capabilities requested, this will result in almost no "
                         "functionality being available");
    }

    // Check: backend_specific_config does not match with graphics_api requested
    switch (config.graphics_api)
    {
    case GraphicsAPI::Vulkan: {
        if (!std::holds_alternative<VulkanSpecificConfiguration>(config.backend_specific_config))
        {
            MIZU_LOG_ERROR("Vulkan API requested but backend_specific_config is not VulkanSpecificConfiguration");
        }
    }
    break;
    case GraphicsAPI::OpenGL: {
        if (!std::holds_alternative<OpenGLSpecificConfiguration>(config.backend_specific_config))
        {
            MIZU_LOG_ERROR("OpenGL API requested but backend_specific_config is not OpenGLSpecificConfiguration");
        }
    }
    break;
    }
}

bool Renderer::initialize(RendererConfiguration config)
{
    MIZU_LOG_SETUP;

    s_config = std::move(config);
    sanity_checks(s_config);

    switch (s_config.graphics_api)
    {
    case GraphicsAPI::Vulkan:
        s_backend = std::make_unique<Vulkan::VulkanBackend>();
        break;
    case GraphicsAPI::OpenGL:
        s_backend = std::make_unique<OpenGL::OpenGLBackend>();
        break;
    }

    s_memory_allocator = BaseDeviceMemoryAllocator::create();

    ShaderManager::create_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_PATH);

    return s_backend->initialize(s_config);
}

void Renderer::shutdown()
{
    wait_idle();

    ShaderManager::clean();

    s_memory_allocator = nullptr;
    s_backend = nullptr;
    s_config = {};
}

void Renderer::wait_idle()
{
    s_backend->wait_idle();
}

std::shared_ptr<IDeviceMemoryAllocator> Renderer::get_allocator()
{
    return s_memory_allocator;
}

RendererConfiguration Renderer::Renderer::get_config()
{
    return s_config;
}

} // namespace Mizu
