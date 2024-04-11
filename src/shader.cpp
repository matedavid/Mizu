#include "shader.h"

#include "renderer.h"

#include "backend/vulkan/vulkan_shader.h"

namespace Mizu {

std::shared_ptr<Shader> Shader::create(const std::filesystem::path& vertex_path,
                                       const std::filesystem::path& fragment_path) {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanShader>(vertex_path, fragment_path);
    }
}

std::shared_ptr<ComputeShader> ComputeShader::create(const std::filesystem::path& path) {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanComputeShader>(path);
    }
}

} // namespace Mizu
