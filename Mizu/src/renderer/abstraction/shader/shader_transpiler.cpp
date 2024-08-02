#include "shader_transpiler.h"

#include <iostream>
#include <spirv_glsl.hpp>

#include "utility/logging.h"

namespace Mizu {

ShaderTranspiler::ShaderTranspiler(const std::vector<char>& content, Translation translation) {
    switch (translation) {
    case Translation::Spirv_2_OpenGL46:
        compile_spirv_2_opengl46(content);
        break;
    }
}

void ShaderTranspiler::compile_spirv_2_opengl46(const std::vector<char>& content) {
    /*
     * Things to do:
     * - Remove descriptors sets
     * - Move all samplers/images to a linear binding pattern (first image 0, second 1...)
     * - Move all uniform buffers to a linear binding pattern
     * - Make sure binding points do not collide, as they correspond with the uniform locations
     */

    const auto* data = reinterpret_cast<const uint32_t*>(content.data());
    spirv_cross::CompilerGLSL glsl{data, content.size() / sizeof(uint32_t)};

    const auto resources = glsl.get_shader_resources();

    // images
    std::vector<spirv_cross::Resource> image_resources;
    image_resources.reserve(resources.sampled_images.size() + resources.storage_images.size());
    image_resources.insert(image_resources.end(), resources.sampled_images.begin(), resources.sampled_images.end());
    image_resources.insert(image_resources.end(), resources.storage_images.begin(), resources.storage_images.end());

    // uniform buffers
    std::vector<spirv_cross::Resource> uniform_resources;
    uniform_resources.reserve(resources.uniform_buffers.size());
    uniform_resources.insert(
        uniform_resources.end(), resources.uniform_buffers.begin(), resources.uniform_buffers.end());

    // push constants
    std::vector<spirv_cross::Resource> push_constant_resources;
    push_constant_resources.reserve(resources.push_constant_buffers.size());
    push_constant_resources.insert(
        push_constant_resources.end(), resources.push_constant_buffers.begin(), resources.push_constant_buffers.end());

    // all resources
    std::vector<spirv_cross::Resource> all_resources;
    all_resources.reserve(image_resources.size() + uniform_resources.size());
    all_resources.insert(all_resources.begin(), image_resources.begin(), image_resources.end());
    all_resources.insert(all_resources.begin(), uniform_resources.begin(), uniform_resources.end());
    all_resources.insert(all_resources.begin(), push_constant_resources.begin(), push_constant_resources.end());

    // Determine binding base for each set
    std::vector<int32_t> max_binding_per_set;
    for (const auto& resource : all_resources) {
        const uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
        if (set >= max_binding_per_set.size()) {
            max_binding_per_set.resize(set + 1, -1);
        }

        const uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
        max_binding_per_set[set] = std::max(max_binding_per_set[set], static_cast<int32_t>(binding));
    }

    std::vector<uint32_t> base_binding_per_set(max_binding_per_set.size());

    if (!max_binding_per_set.empty()) {
        base_binding_per_set[0] = static_cast<uint32_t>(std::max(max_binding_per_set[0], 0));

        for (size_t i = 1; i < base_binding_per_set.size(); ++i) {
            if (max_binding_per_set[i] != -1) {
                base_binding_per_set[i] =
                    base_binding_per_set[i - 1] + static_cast<unsigned int>(max_binding_per_set[i]);
            }
        }
    }

    // Configure images
    for (const auto& resource : image_resources) {
        const uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
        const uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

        const uint32_t new_binding = base_binding_per_set[set] + binding;

        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
        glsl.set_decoration(resource.id, spv::DecorationBinding, new_binding);
    }

    // Configure uniforms
    uint32_t biggest_uniform_binding = 0;
    for (const auto& resource : uniform_resources) {
        const uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
        const uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

        const uint32_t new_binding = base_binding_per_set[set] + binding;

        biggest_uniform_binding = std::max(new_binding, biggest_uniform_binding);

        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
        glsl.set_decoration(resource.id, spv::DecorationBinding, new_binding);

        glsl.set_name(resource.base_type_id, glsl.get_name(resource.id));
        glsl.set_name(resource.id, glsl.get_block_fallback_name(resource.id));
    }

    // Configure uniforms
    biggest_uniform_binding += 1;
    for (const auto& resource : push_constant_resources) {
        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
        glsl.set_decoration(resource.id, spv::DecorationBinding, biggest_uniform_binding++);

        glsl.set_name(resource.base_type_id, glsl.get_name(resource.id));
        glsl.set_name(resource.id, glsl.get_block_fallback_name(resource.id));
    }

    spirv_cross::CompilerGLSL::Options options;
    options.version = 460;
    options.emit_push_constant_as_uniform_buffer = true;
    options.es = false;
    glsl.set_common_options(options);

    m_compilation = glsl.compile();
}

} // namespace Mizu