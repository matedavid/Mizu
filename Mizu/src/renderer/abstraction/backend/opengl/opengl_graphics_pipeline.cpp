#include "opengl_graphics_pipeline.h"

#include "renderer/abstraction/renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_buffer_resource.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu::OpenGL {

OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const Description& desc) : m_description(desc) {
    m_shader = std::dynamic_pointer_cast<OpenGLGraphicsShader>(m_description.shader);

    glGenVertexArrays(1, &m_vao);
}

OpenGLGraphicsPipeline::~OpenGLGraphicsPipeline() {
    glDeleteVertexArrays(1, &m_vao);
}

void OpenGLGraphicsPipeline::set_state() const {
    glUseProgram(m_shader->handle());
    glBindVertexArray(m_vao);

    const auto enable_on_boolean = [](GLenum cap, bool val) {
        if (val)
            glEnable(cap);
        else
            glDisable(cap);
    };

    // Rasterization
    {
        const auto& rasterization = m_description.rasterization;

        enable_on_boolean(GL_RASTERIZER_DISCARD, rasterization.rasterizer_discard);
        glPolygonMode(GL_FRONT_AND_BACK, get_polygon_mode(rasterization.polygon_mode));

        glEnable(GL_CULL_FACE);
        glCullFace(get_cull_mode(rasterization.cull_mode));
        glFrontFace(get_front_face(rasterization.front_face));

        if (rasterization.depth_bias.enabled) {
            glEnable(get_depth_bias_polygon_mode(rasterization.polygon_mode));
            glPolygonOffset(rasterization.depth_bias.constant_factor, rasterization.depth_bias.slope_factor);
        } else {
            glDisable(get_depth_bias_polygon_mode(rasterization.polygon_mode));
        }
    }

    // Depth stencil
    {
        const auto& depth_stencil = m_description.depth_stencil;

        enable_on_boolean(GL_DEPTH_TEST, depth_stencil.depth_test);
        glDepthMask(depth_stencil.depth_write);
        glDepthFunc(get_depth_func(depth_stencil.depth_compare_op));

        // TODO: Investigate depth bounds, seems like it is an extension
        // (https://registry.khronos.org/OpenGL/extensions/EXT/EXT_depth_bounds_test.txt)
    }

    // Color blend
    static bool s_warning_shown = false;
    {
        if (!s_warning_shown) {
            MIZU_LOG_WARNING("OpenGL::GraphicsPipeline color blending not implemented");
            s_warning_shown = true;
        }
    }
}

void OpenGLGraphicsPipeline::push_constant(std::string_view name, uint32_t size, const void* data) {
    const auto info = m_shader->get_constant(name);
    MIZU_ASSERT(info.has_value(), "Push constant '{}' not found in GraphicsPipeline", name);

    MIZU_ASSERT(info->size == size,
                "Size of provided data and size of push constant do not match ({} != {})",
                size,
                info->size);

    auto constant_it = m_constants.find(std::string{name});
    if (constant_it == m_constants.end()) {
        BufferDescription buffer_desc{};
        buffer_desc.size = size;
        buffer_desc.type = BufferType::UniformBuffer;

        auto ub = std::make_shared<OpenGLBufferResource>(buffer_desc);
        constant_it = m_constants.insert({std::string{name}, ub}).first;
    }

    constant_it->second->set_data(reinterpret_cast<const uint8_t*>(data));

    const auto binding_point = m_shader->get_uniform_location(name);
    MIZU_ASSERT(binding_point.has_value(), "Constant binding point invalid");
    glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(*binding_point), constant_it->second->handle());
}

void OpenGLGraphicsPipeline::set_vertex_buffer_layout() {
    // TODO: HACK, should revisit and think better way

    const std::vector<ShaderInput>& inputs = m_shader->get_inputs();

    uint32_t generic_stride = 0;
    for (const ShaderInput& element : inputs) {
        generic_stride += ShaderType::size(element.type);
    }

    uint32_t stride = 0;
    for (uint32_t i = 0; i < inputs.size(); ++i) {
        const ShaderInput& element = inputs[i];
        glVertexAttribPointer(i,
                              static_cast<GLint>(get_type_count(element.type)),
                              get_opengl_type(element.type),
                              GL_FALSE, // TODO: element.normalized ? GL_TRUE : GL_FALSE,
                              static_cast<GLsizei>(generic_stride),
                              reinterpret_cast<const void*>(stride));
        glEnableVertexAttribArray(i);

        stride += ShaderType::size(element.type);
    }
}

GLenum OpenGLGraphicsPipeline::get_polygon_mode(RasterizationState::PolygonMode mode) {
    using PolygonMode = RasterizationState::PolygonMode;

    switch (mode) {
    case PolygonMode::Fill:
        return GL_FILL;
    case PolygonMode::Line:
        return GL_LINE;
    case PolygonMode::Point:
        return GL_POINT;
    }
}

GLenum OpenGLGraphicsPipeline::get_cull_mode(RasterizationState::CullMode mode) {
    using CullMode = RasterizationState::CullMode;

    switch (mode) {
    case CullMode::None:
        return GL_NONE;
    case CullMode::Front:
        return GL_FRONT;
    case CullMode::Back:
        return GL_BACK;
    case CullMode::FrontAndBack:
        return GL_FRONT_AND_BACK;
    }
}

GLenum OpenGLGraphicsPipeline::get_front_face(RasterizationState::FrontFace face) {
    using FrontFace = RasterizationState::FrontFace;

    switch (face) {
    case FrontFace::CounterClockwise:
        return GL_CCW;
    case FrontFace::ClockWise:
        return GL_CW;
    }
}

GLenum OpenGLGraphicsPipeline::get_depth_bias_polygon_mode(RasterizationState::PolygonMode mode) {
    using PolygonMode = RasterizationState::PolygonMode;

    switch (mode) {
    case PolygonMode::Fill:
        return GL_POLYGON_OFFSET_FILL;
    case PolygonMode::Line:
        return GL_POLYGON_OFFSET_LINE;
    case PolygonMode::Point:
        return GL_POLYGON_OFFSET_POINT;
    }
}

GLenum OpenGLGraphicsPipeline::get_depth_func(DepthStencilState::DepthCompareOp op) {
    using DepthCompareOp = DepthStencilState::DepthCompareOp;

    switch (op) {
    case DepthCompareOp::Never:
        return GL_NEVER;
    case DepthCompareOp::Less:
        return GL_LESS;
    case DepthCompareOp::Equal:
        return GL_EQUAL;
    case DepthCompareOp::LessEqual:
        return GL_LEQUAL;
    case DepthCompareOp::Greater:
        return GL_GREATER;
    case DepthCompareOp::NotEqual:
        return GL_NOTEQUAL;
    case DepthCompareOp::GreaterEqual:
        return GL_GEQUAL;
    case DepthCompareOp::Always:
        return GL_ALWAYS;
    }
}

GLenum OpenGLGraphicsPipeline::get_opengl_type(ShaderType type) {
    switch (type) {
    case ShaderType::Float:
    case ShaderType::Float2:
    case ShaderType::Float3:
    case ShaderType::Float4:
        return GL_FLOAT;
    default:
        MIZU_UNREACHABLE("Type not valid");
    }
}

uint32_t OpenGLGraphicsPipeline::get_type_count(ShaderType type) {
    switch (type) {
    case ShaderType::Float:
        return 1;
    case ShaderType::Float2:
        return 2;
    case ShaderType::Float3:
        return 3;
    case ShaderType::Float4:
        return 4;
    default:
        MIZU_UNREACHABLE("Type not valid");
    }
}

} // namespace Mizu::OpenGL
