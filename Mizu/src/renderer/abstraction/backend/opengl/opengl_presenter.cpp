#include "opengl_presenter.h"

#include <cassert>

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_context.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"
#include "renderer/abstraction/backend/opengl/opengl_texture.h"

#include "managers/shader_manager.h"

namespace Mizu::OpenGL {

OpenGLPresenter::OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
      : m_window(std::move(window)) {
    m_present_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(std::move(texture));
    assert(m_present_texture != nullptr && "Could not convert Texture2D to OpenGLTexture2D");

    m_present_shader = std::dynamic_pointer_cast<OpenGLGraphicsShader>(ShaderManager::get_shader(
        "/EngineShaders/presenter/present.vert.spv", "/EngineShaders/presenter/present.frag.spv"));

    m_texture_location = glGetUniformLocation(m_present_shader->handle(), "uPresentTexture");
    assert(m_texture_location != -1 && "Could not find uPresentTexture location");

    m_vertex_buffer =
        std::dynamic_pointer_cast<OpenGLVertexBuffer>(VertexBuffer::create(m_vertex_data, m_vertex_layout));
}

OpenGLPresenter::~OpenGLPresenter() = default;

void OpenGLPresenter::present() {
    present(nullptr);
}

void OpenGLPresenter::present([[maybe_unused]] const std::shared_ptr<Semaphore>& wait_semaphore) {
    GL_DEBUG_BEGIN_LABEL("Presentation");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDisable(GL_DEPTH_TEST);

    glUseProgram(m_present_shader->handle());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_present_texture->handle());

    glUniform1i(m_texture_location, 0);

    m_vertex_buffer->bind();
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_buffer->count()));

    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST);

    GL_DEBUG_ENDL_LABEL();
}

void OpenGLPresenter::texture_changed(std::shared_ptr<Texture2D> texture) {
    m_present_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(std::move(texture));
    assert(m_present_texture != nullptr && "Texture cannot be nullptr");
}

} // namespace Mizu::OpenGL