#include "opengl_presenter.h"

#include <cassert>

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"
#include "renderer/abstraction/backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

OpenGLPresenter::OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
      : m_window(window) {
    m_present_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(texture);
    assert(m_present_texture != nullptr && "Could not convert Texture2D to OpenGLTexture2D");

    m_present_shader =
        std::make_shared<OpenGLShader>("../../Mizu/shaders/present.vert.spv", "../../Mizu/shaders/present.frag.spv");

    const auto vertex_buffer = VertexBuffer::create(vertex_data, vertex_layout);
    m_vertex_buffer = std::dynamic_pointer_cast<OpenGLVertexBuffer>(std::move(vertex_buffer));
}

OpenGLPresenter::~OpenGLPresenter() {}

void OpenGLPresenter::present() {
    present(nullptr);
}

void OpenGLPresenter::present(const std::shared_ptr<Semaphore>& wait_semaphore) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glUseProgram(m_present_shader->handle());

    const GLint location = glGetUniformLocation(m_present_shader->handle(), "uPresentTexture");
    assert(location != -1 && "Could not find uPresentTexture location");

    glActiveTexture(GL_TEXTURE0);
    glUniform1d(location, 0);

    glBindTexture(GL_TEXTURE_2D, m_present_texture->handle());

    m_vertex_buffer->bind();
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_buffer->count()));
}

void OpenGLPresenter::window_resized(uint32_t width, uint32_t height) {}

} // namespace Mizu::OpenGL