#include "renderer.h"

#include <memory>

#include "base/debug/logging.h"

#include "managers/shader_manager.h"

#include "render_core/rhi/backend/opengl/opengl_backend.h"
#include "render_core/rhi/backend/vulkan/vulkan_backend.h"

namespace Mizu
{

#ifndef MIZU_ENGINE_SHADERS_PATH
#define MIZU_ENGINE_SHADERS_PATH ""
#endif

static std::unique_ptr<IBackend> s_backend = nullptr;
static std::shared_ptr<BaseDeviceMemoryAllocator> s_memory_allocator = nullptr;
static std::shared_ptr<PipelineCache> s_pipeline_cache = nullptr;
static std::shared_ptr<SamplerStateCache> s_sampler_state_cache = nullptr;
static RendererConfiguration s_config = {};
static RendererCapabilities s_capabilities = {};

static void sanity_checks(const RendererConfiguration& config)
{
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
    s_pipeline_cache = std::make_shared<PipelineCache>();
    s_sampler_state_cache = std::make_shared<SamplerStateCache>();

    ShaderManager::create_shader_mapping("EngineShaders", MIZU_ENGINE_SHADERS_PATH);

    const bool success = s_backend->initialize(s_config);
    if (success)
    {
        s_capabilities = s_backend->get_capabilities();

        MIZU_LOG_INFO("Device capabilities:");
        MIZU_LOG_INFO("    Max resource group sets: {}", s_capabilities.max_resource_group_sets);
        MIZU_LOG_INFO("    Max push constant size:  {}", s_capabilities.max_push_constant_size);
        MIZU_LOG_INFO("    Ray tracing hardware:    {}", s_capabilities.ray_tracing_hardware);
    }

    return success;
}

void Renderer::shutdown()
{
    wait_idle();

    ShaderManager::clean();

    s_sampler_state_cache = nullptr;
    s_pipeline_cache = nullptr;
    s_memory_allocator = nullptr;
    s_backend = nullptr;
    s_config = {};
    s_capabilities = {};
}

void Renderer::wait_idle()
{
    s_backend->wait_idle();
}

std::shared_ptr<IDeviceMemoryAllocator> Renderer::get_allocator()
{
    return s_memory_allocator;
}

std::shared_ptr<PipelineCache> Renderer::get_pipeline_cache()
{
    return s_pipeline_cache;
}

std::shared_ptr<SamplerStateCache> Renderer::get_sampler_state_cache()
{
    return s_sampler_state_cache;
}

RendererConfiguration Renderer::get_config()
{
    return s_config;
}

RendererCapabilities Renderer::get_capabilities()
{
    return s_capabilities;
}

} // namespace Mizu
