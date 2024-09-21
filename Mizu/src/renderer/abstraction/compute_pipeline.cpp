#include "compute_pipeline.h"

#include "renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_compute_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_compute_pipeline.h"

namespace Mizu {

std::shared_ptr<ComputePipeline> ComputePipeline::create(const Description& desc) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanComputePipeline>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLComputePipeline>(desc);
    }
}

} // namespace Mizu