#include "resource_group.h"

#include "renderer.h"

#include "backend/opengl/opengl_resource_group.h"
#include "backend/vulkan/vulkan_resource_group.h"

namespace Mizu {

std::shared_ptr<ResourceGroup> ResourceGroup::create() {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanResourceGroup>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLResourceGroup>();
    }
}

} // namespace Mizu
