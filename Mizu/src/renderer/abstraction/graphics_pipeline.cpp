#include "graphics_pipeline.h"

#include "renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_graphics_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_graphics_pipeline.h"

namespace Mizu {

std::shared_ptr<GraphicsPipeline> GraphicsPipeline::create(const Description& desc) {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanGraphicsPipeline>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLGraphicsPipeline>(desc);
    }
}

} // namespace Mizu
