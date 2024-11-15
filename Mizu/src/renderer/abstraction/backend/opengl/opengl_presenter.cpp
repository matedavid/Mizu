#include "opengl_presenter.h"

#include <cassert>

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_context.h"
#include "renderer/abstraction/backend/opengl/opengl_image_resource.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"

#include "managers/shader_manager.h"

#include "utility/assert.h"

namespace Mizu::OpenGL {

OpenGLPresenter::OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
      : m_window(std::move(window)) {
    m_present_texture = std::dynamic_pointer_cast<OpenGLImageResource>(texture->get_resource());
    MIZU_ASSERT(m_present_texture != nullptr, "Could not convert Texture2D to OpenGLTexture2D");

    m_present_shader = std::dynamic_pointer_cast<OpenGLGraphicsShader>(ShaderManager::get_shader(
        {"/EngineShaders/presenter/present.vert.spv", "main"}, {"/EngineShaders/presenter/present.frag.spv", "main"}));

    m_texture_location = glGetUniformLocation(m_present_shader->handle(), "uPresentTexture");
    MIZU_ASSERT(m_texture_location != -1, "Could not find uPresentTexture location");

    const std::vector<PresenterVertex> vertex_data = {
        {.position = glm::vec3(-1.0f, -1.0f, 0.0f), .texture_coords = {0.0f, 0.0f}},
        {.position = glm::vec3(1.0f, -1.0f, 0.0f), .texture_coords = {1.0f, 0.0f}},
        {.position = glm::vec3(1.0f, 1.0f, 0.0f), .texture_coords = {1.0f, 1.0f}},

        {.position = glm::vec3(1.0f, 1.0f, 0.0f), .texture_coords = {1.0f, 1.0f}},
        {.position = glm::vec3(-1.0f, 1.0f, 0.0f), .texture_coords = {0.0f, 1.0f}},
        {.position = glm::vec3(-1.0f, -1.0f, 0.0f), .texture_coords = {0.0f, 0.0f}},
    };

    std::vector<VertexBuffer::Layout> vertex_layout = {
        {.type = VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
        {.type = VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
    };

    m_vertex_buffer = std::dynamic_pointer_cast<OpenGLVertexBuffer>(VertexBuffer::create(vertex_data, vertex_layout));
}

OpenGLPresenter::~OpenGLPresenter() = default;

void OpenGLPresenter::present() {
    present(nullptr);
}

void OpenGLPresenter::present([[maybe_unused]] const std::shared_ptr<Semaphore>& wait_semaphore) {
    GL_DEBUG_BEGIN_LABEL("Presentation");
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glFrontFace(GL_CCW);

        glUseProgram(m_present_shader->handle());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_present_texture->handle());

        glUniform1i(m_texture_location, 0);

        m_vertex_buffer->bind();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_buffer->count()));

        glBindTexture(GL_TEXTURE_2D, 0);

        glEnable(GL_DEPTH_TEST);
    }
    GL_DEBUG_END_LABEL();
}

void OpenGLPresenter::texture_changed(std::shared_ptr<Texture2D> texture) {
    m_present_texture = std::dynamic_pointer_cast<OpenGLImageResource>(texture->get_resource());
    MIZU_ASSERT(m_present_texture != nullptr, "Texture cannot be nullptr");
}

} // namespace Mizu::OpenGL
