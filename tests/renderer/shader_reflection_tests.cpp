#include "renderer_tests_common.h"
#include "resources_manager.h"

#include "renderer/abstraction/shader/shader_reflection.h"
#include "utility/filesystem.h"

TEST_CASE("ShaderReflection Tests", "[ShaderReflection]") {
    SECTION("Graphics Shader 1") {
        {
            const auto vertex_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.vert.spv");
            const auto vertex_source = Mizu::Filesystem::read_file(vertex_shader_path);

            Mizu::ShaderReflection vertex_reflection(vertex_source);

            const auto& inputs = vertex_reflection.get_inputs();
            REQUIRE(inputs.size() == 2);

            REQUIRE(inputs[0].name == "uPosition");
            REQUIRE(inputs[0].type == Mizu::ShaderType::Vec3);

            REQUIRE(inputs[1].name == "uTexCoord");
            REQUIRE(inputs[1].type == Mizu::ShaderType::Vec2);
        }

        {
            const auto fragment_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.frag.spv");
            const auto fragment_source = Mizu::Filesystem::read_file(fragment_shader_path);

            Mizu::ShaderReflection fragment_reflection(fragment_source);

            const auto& properties = fragment_reflection.get_properties();
            REQUIRE(properties.size() == 3);

            REQUIRE(properties[0].name == "uTexture1");
            REQUIRE(std::holds_alternative<Mizu::ShaderTextureProperty2>(properties[0].type));
            REQUIRE(properties[0].binding_info.set == 0);
            REQUIRE(properties[0].binding_info.binding == 0);

            REQUIRE(properties[1].name == "uTexture2");
            REQUIRE(std::holds_alternative<Mizu::ShaderTextureProperty2>(properties[1].type));
            REQUIRE(properties[1].binding_info.set == 1);
            REQUIRE(properties[1].binding_info.binding == 0);

            REQUIRE(properties[2].name == "Uniform1"); // TODO: INCORRECT, should be uUniform1
            REQUIRE(std::holds_alternative<Mizu::ShaderBufferProperty2>(properties[2].type));
            REQUIRE(properties[2].binding_info.set == 1);
            REQUIRE(properties[2].binding_info.binding == 1);
        }
    }
}
