#pragma once

#include <filesystem>
#include <glad/glad.h>

#include "renderer/abstraction/backend/opengl/opengl_image.h"
#include "renderer/abstraction/cubemap.h"

namespace Mizu::OpenGL {

class OpenGLCubemap : public OpenGLImage, public Cubemap {
  public:
    OpenGLCubemap(const ImageDescription& desc);
    OpenGLCubemap(const Faces& faces);

  private:
    void load_face(const std::filesystem::path& path, uint32_t face, uint32_t& width, uint32_t& height) const;
};

} // namespace Mizu::OpenGL