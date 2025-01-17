#include "opengl_presenter.h"

#include <cassert>

#include "render_core/resources/buffers.h"
#include "render_core/resources/texture.h"

#include "render_core/rhi/backend/opengl/opengl_buffer_resource.h"
#include "render_core/rhi/backend/opengl/opengl_context.h"
#include "render_core/rhi/backend/opengl/opengl_graphics_pipeline.h"
#include "render_core/rhi/backend/opengl/opengl_image_resource.h"
#include "render_core/rhi/backend/opengl/opengl_shader.h"

#include "managers/shader_manager.h"

#include "utility/assert.h"

namespace Mizu::OpenGL
{

OpenGLPresenter::OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
    : m_window(std::move(window))
{
    m_present_texture = std::dynamic_pointer_cast<OpenGLImageResource>(texture->get_resource());
    MIZU_ASSERT(m_present_texture != nullptr, "Could not convert Texture2D to OpenGLTexture2D");

    const auto shader = std::dynamic_pointer_cast<OpenGLGraphicsShader>(
        ShaderManager::get_shader({"/EngineShaders/presenter/Present.vert.spv", "vsMain"},
                                  {"/EngineShaders/presenter/Present.frag.spv", "fsMain"}));

    GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.shader = shader;
    pipeline_desc.depth_stencil = DepthStencilState{
        .depth_test = false,
        .depth_write = false,
    };
    pipeline_desc.rasterization = RasterizationState{
        .front_face = RasterizationState::FrontFace::CounterClockwise,
    };

    const auto pipeline = GraphicsPipeline::create(pipeline_desc);
    m_pipeline = std::dynamic_pointer_cast<OpenGLGraphicsPipeline>(pipeline);

    m_texture_location = glGetUniformLocation(shader->handle(), "uPresentTexture");
    MIZU_ASSERT(m_texture_location != -1, "Could not find uPresentTexture location");

    const std::vector<PresenterVertex> vertex_data = {
        {.position = glm::vec3(-1.0f, -1.0f, 0.0f), .texture_coords = {0.0f, 0.0f}},
        {.position = glm::vec3(1.0f, -1.0f, 0.0f), .texture_coords = {1.0f, 0.0f}},
        {.position = glm::vec3(1.0f, 1.0f, 0.0f), .texture_coords = {1.0f, 1.0f}},

        {.position = glm::vec3(1.0f, 1.0f, 0.0f), .texture_coords = {1.0f, 1.0f}},
        {.position = glm::vec3(-1.0f, 1.0f, 0.0f), .texture_coords = {0.0f, 1.0f}},
        {.position = glm::vec3(-1.0f, -1.0f, 0.0f), .texture_coords = {0.0f, 0.0f}},
    };

    const auto vertex_buffer = VertexBuffer::create(vertex_data, Renderer::get_allocator());

    m_vertex_buffer = std::dynamic_pointer_cast<OpenGLBufferResource>(vertex_buffer->get_resource());
    m_vertex_buffer_count = vertex_buffer->get_count();
}

OpenGLPresenter::~OpenGLPresenter() = default;

void OpenGLPresenter::present()
{
    present(nullptr);
}

void OpenGLPresenter::present([[maybe_unused]] const std::shared_ptr<Semaphore>& wait_semaphore)
{
    GL_DEBUG_BEGIN_LABEL("Presentation");
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        m_pipeline->set_state();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_present_texture->handle());

        glUniform1i(m_texture_location, 0);

        glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer->handle());
        m_pipeline->set_vertex_buffer_layout();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertex_buffer_count));

        glBindTexture(GL_TEXTURE_2D, 0);
    }
    GL_DEBUG_END_LABEL();
}

void OpenGLPresenter::texture_changed(std::shared_ptr<Texture2D> texture)
{
    m_present_texture = std::dynamic_pointer_cast<OpenGLImageResource>(texture->get_resource());
    MIZU_ASSERT(m_present_texture != nullptr, "Texture cannot be nullptr");
}

} // namespace Mizu::OpenGL
