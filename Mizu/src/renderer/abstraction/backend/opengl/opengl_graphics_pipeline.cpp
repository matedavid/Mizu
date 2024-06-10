#include "opengl_graphics_pipeline.h"

#include "utility/logging.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"
#include "renderer/abstraction/backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const Description& desc) : m_description(desc) {
    m_shader = std::dynamic_pointer_cast<OpenGLShader>(m_description.shader);

    // Only getting properties because in OpenGL constants == properties
    for (const auto& property : m_shader->get_properties()) {
        std::string name;
        if (std::holds_alternative<ShaderTextureProperty>(property)) {
            name = std::get<ShaderTextureProperty>(property).name;
        } else if (std::holds_alternative<ShaderUniformBufferProperty>(property)) {
            name = std::get<ShaderUniformBufferProperty>(property).name;
        } else if (std::holds_alternative<ShaderValueProperty>(property)) {
            name = std::get<ShaderValueProperty>(property).name;
        }

        m_uniform_info.insert({name, std::nullopt});
    }
}

void OpenGLGraphicsPipeline::set_state() const {
    glUseProgram(m_shader->handle());

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

bool OpenGLGraphicsPipeline::push_constant([[maybe_unused]] const std::shared_ptr<ICommandBuffer>& command_buffer,
                                           std::string_view name,
                                           uint32_t size,
                                           const void* data) {
    const auto info = get_uniform_info(name, OpenGLUniformType::UniformBuffer, "UniformBuffer");
    if (!info.has_value())
        return false;

    if (info->size != size) {
        MIZU_LOG_ERROR("Size of provided data and size of push constant do not match ({} != {})", info->size, size);
        return false;
    }

    auto constant_it = m_constants.find(std::string{name});
    if (constant_it == m_constants.end()) {
        auto ub = std::make_shared<OpenGLUniformBuffer>(size);
        constant_it = m_constants.insert({std::string{name}, ub}).first;
    }

    constant_it->second->set_data(data);
    glBindBufferBase(GL_UNIFORM_BUFFER, info->binding, constant_it->second->handle());

    return true;
}

std::optional<OpenGLUniformInfo> OpenGLGraphicsPipeline::get_uniform_info(std::string_view name,
                                                                          OpenGLUniformType type,
                                                                          std::string_view type_name) const {
    const auto info = m_shader->get_uniform_info(name);
    if (!info.has_value()) {
        MIZU_LOG_WARNING("Property '{}' not found in GraphicsPipeline", name);
        return std::nullopt;
    }

    if (info->type != type) {
        MIZU_LOG_WARNING("Property '{}' is not of type {}", name, type_name);
        return std::nullopt;
    }

    return info;
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

} // namespace Mizu::OpenGL