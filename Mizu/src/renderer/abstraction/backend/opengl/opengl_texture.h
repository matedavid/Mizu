#pragma once

#include <glad/glad.h>

#include "renderer/abstraction/backend/opengl/opengl_image.h"
#include "renderer/abstraction/texture.h"

namespace Mizu::OpenGL {

class OpenGLTexture2D : public OpenGLImage, public Texture2D {
  public:
    explicit OpenGLTexture2D(const ImageDescription& desc);
};

} // namespace Mizu::OpenGL
