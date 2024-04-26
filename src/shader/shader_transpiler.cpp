#include "shader_transpiler.h"

#include <spirv_glsl.hpp>

namespace Mizu {

static std::string compile_vulkan_2_opengl46(spirv_cross::CompilerGLSL& glsl);

ShaderTranspiler::ShaderTranspiler(const std::vector<char>& content, Translation translation) {
    const auto* data = reinterpret_cast<const uint32_t*>(content.data());

    spirv_cross::CompilerGLSL glsl{data, content.size() / sizeof(uint32_t)};

    switch (translation) {
    case Translation::Vulkan_2_OpenGL46:
        m_compilation = compile_vulkan_2_opengl46(glsl);
        break;
    }
}

std::string compile_vulkan_2_opengl46(spirv_cross::CompilerGLSL& glsl) {
    /*
     * Things to do:
     * - Remove descriptors sets
     * - Move all samplers/images to a linear binding pattern (first image 0, second 1...)
     * - Move all uniform buffers to a linear binding pattern
     * - Make sure binding points do not collide, as they correspond with the uniform locations
     */

    const auto resources = glsl.get_shader_resources();
    uint32_t binding_point = 0;

    // images
    std::vector<spirv_cross::Resource> image_resources;
    image_resources.reserve(resources.sampled_images.size() + resources.storage_images.size());
    image_resources.insert(image_resources.end(), resources.sampled_images.begin(), resources.sampled_images.end());
    image_resources.insert(image_resources.end(), resources.storage_images.begin(), resources.storage_images.end());

    for (const auto& resource : image_resources) {
        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
        glsl.set_decoration(resource.id, spv::DecorationBinding, binding_point);

        binding_point += 1;
    }

    // uniform buffer
    std::vector<spirv_cross::Resource> uniform_resources;
    uniform_resources.reserve(resources.uniform_buffers.size() + resources.push_constant_buffers.size());
    uniform_resources.insert(
        uniform_resources.end(), resources.uniform_buffers.begin(), resources.uniform_buffers.end());
    uniform_resources.insert(
        uniform_resources.end(), resources.push_constant_buffers.begin(), resources.push_constant_buffers.end());

    for (const auto& resource : uniform_resources) {
        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
        glsl.set_decoration(resource.id, spv::DecorationBinding, binding_point);

        glsl.set_name(resource.base_type_id, glsl.get_name(resource.id));
        glsl.set_name(resource.id, glsl.get_block_fallback_name(resource.id));

        binding_point += 1;
    }

    spirv_cross::CompilerGLSL::Options options;
    options.version = 460;
    options.emit_push_constant_as_uniform_buffer = true;
    options.es = false;
    glsl.set_common_options(options);

    return glsl.compile();
}

} // namespace Mizu