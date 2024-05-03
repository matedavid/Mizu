#include "opengl_resource_group.h"

#include "backend/opengl/opengl_buffers.h"
#include "backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) {
    const auto native_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(texture);
    // TODO:
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<OpenGLUniformBuffer>(ubo);
    // TODO:
}

} // namespace Mizu::OpenGL
