#include "presenter.h"

#include "renderer/abstraction/renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_presenter.h"
#include "renderer/abstraction/backend/vulkan/vulkan_presenter.h"

namespace Mizu {

std::shared_ptr<Presenter> Presenter::create(const std::shared_ptr<Window>& window,
                                             const std::shared_ptr<Texture2D>& texture) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanPresenter>(window, texture);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLPresenter>(window, texture);
    }
}

} // namespace Mizu
