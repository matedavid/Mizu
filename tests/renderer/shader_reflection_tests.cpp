#include "renderer_tests_common.h"
#include "resources_manager.h"

#include "renderer/shader/shader_reflection.h"
#include "utility/filesystem.h"

TEST_CASE("ShaderReflection Tests", "[ShaderReflection]") {
    /*
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
            REQUIRE(std::holds_alternative<Mizu::ShaderTextureProperty>(properties[0].value));
            REQUIRE(properties[0].binding_info.set == 0);
            REQUIRE(properties[0].binding_info.binding == 0);

            REQUIRE(std::get<Mizu::ShaderTextureProperty>(properties[0].value).type
                    == Mizu::ShaderTextureProperty::Type::Sampled);

            REQUIRE(properties[1].name == "uTexture2");
            REQUIRE(std::holds_alternative<Mizu::ShaderTextureProperty>(properties[1].value));
            REQUIRE(properties[1].binding_info.set == 1);
            REQUIRE(properties[1].binding_info.binding == 0);

            REQUIRE(std::get<Mizu::ShaderTextureProperty>(properties[1].value).type
                    == Mizu::ShaderTextureProperty::Type::Sampled);

            REQUIRE(properties[2].name == "uUniform1");
            REQUIRE(std::holds_alternative<Mizu::ShaderBufferProperty>(properties[2].value));
            REQUIRE(properties[2].binding_info.set == 1);
            REQUIRE(properties[2].binding_info.binding == 1);

            {
                const auto& value = std::get<Mizu::ShaderBufferProperty>(properties[2].value);

                REQUIRE(value.type == Mizu::ShaderBufferProperty::Type::Uniform);
                REQUIRE(value.members.size() == 2);
                REQUIRE(value.total_size
                        == Mizu::ShaderType::padded_size(Mizu::ShaderType::Vec4)
                               + Mizu::ShaderType::padded_size(Mizu::ShaderType::Vec3));

                REQUIRE(value.members[0].name == "Position");
                REQUIRE(value.members[0].type == Mizu::ShaderType::Vec4);

                REQUIRE(value.members[1].name == "Direction");
                REQUIRE(value.members[1].type == Mizu::ShaderType::Vec3);
            }

            const auto& constants = fragment_reflection.get_constants();
            REQUIRE(constants.size() == 1);

            REQUIRE(constants[0].name == "uConstant1");
            REQUIRE(constants[0].size == Mizu::ShaderType::size(Mizu::ShaderType::Vec4));
        }
    }
    */

    SECTION("Types Shader 1") {
        const std::string& path = GENERATE("ReflectionShader.slang.spv", "ReflectionShader.glsl.spv");

        const auto resources_path = ResourcesManager::get_resource_path(path);
        const auto vertex_source = Mizu::Filesystem::read_file(resources_path);

        Mizu::ShaderReflection reflection(vertex_source);

        const std::vector<Mizu::ShaderProperty>& properties = reflection.get_properties();
        REQUIRE((properties.size() == 1 && properties[0].name == "types"));
        const Mizu::ShaderProperty& types_property = properties[0];
        REQUIRE(std::holds_alternative<Mizu::ShaderBufferProperty>(types_property.value));

        const auto has_member =
            [&](const Mizu::ShaderBufferProperty& prop, const std::string& name, Mizu::ShaderType type) {
                for (const Mizu::ShaderMemberProperty& member : prop.members) {
                    if (member.name == name) {
                        MIZU_LOG_INFO("{} {}", member.name, std::string(member.type));
                        return member.type == type;
                    }
                }

                return false;
            };

        const auto& types = std::get<Mizu::ShaderBufferProperty>(types_property.value);

        REQUIRE(has_member(types, "f", Mizu::ShaderType::Float));
        REQUIRE(has_member(types, "f2", Mizu::ShaderType::Float2));
        REQUIRE(has_member(types, "f3", Mizu::ShaderType::Float3));
        REQUIRE(has_member(types, "f4", Mizu::ShaderType::Float4));

        REQUIRE(has_member(types, "f3x3", Mizu::ShaderType::Float3x3));
        REQUIRE(has_member(types, "f4x4", Mizu::ShaderType::Float4x4));
    }
}
