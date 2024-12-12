#include "shader.h"

#include "renderer.h"

#include "render_core/rhi/backend/opengl/opengl_shader.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu
{

std::shared_ptr<GraphicsShader> GraphicsShader::create(const ShaderStageInfo& vert_info,
                                                       const ShaderStageInfo& frag_info)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanGraphicsShader>(vert_info, frag_info);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLGraphicsShader>(vert_info, frag_info);
    }
}

std::shared_ptr<ComputeShader> ComputeShader::create(const ShaderStageInfo& comp_info)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanComputeShader>(comp_info);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLComputeShader>(comp_info);
    }
}

} // namespace Mizu
