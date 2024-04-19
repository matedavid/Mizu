#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

#include "resources_manager.h"

template <typename T>
concept IsShaderPropertyType =
    std::is_same_v<T, Mizu::ShaderTextureProperty> || std::is_same_v<T, Mizu::ShaderUniformBufferProperty>
    || std::is_same_v<T, Mizu::ShaderValueProperty>;

template <typename T>
    requires IsShaderPropertyType<T>
static T get_shader_property(const Mizu::ShaderProperty& value) {
    REQUIRE(std::holds_alternative<T>(value));

    return std::get<T>(value);
}

TEST_CASE("Vulkan Shader", "[Shader]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::initialize(config));

    SECTION("Graphics Shader 1") {
        const auto vertex_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.Vert.spv");
        const auto fragment_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.Frag.spv");

        const auto graphics_shader = Mizu::Shader::create(vertex_shader_path, fragment_shader_path);

        SECTION("Shader compiles correctly") {
            REQUIRE(graphics_shader != nullptr);
        }

        SECTION("Graphics Shader has correct properties") {
            const auto properties = graphics_shader->get_properties();
            REQUIRE(properties.size() == 3);

            auto texture1 = graphics_shader->get_property("uTexture1");
            REQUIRE(texture1.has_value());
            get_shader_property<Mizu::ShaderTextureProperty>(*texture1);

            auto texture2 = graphics_shader->get_property("uTexture2");
            REQUIRE(texture2.has_value());
            get_shader_property<Mizu::ShaderTextureProperty>(*texture2);

            auto ub1 = graphics_shader->get_property("uUniform1");
            REQUIRE(ub1.has_value());

            auto ub1_prop = get_shader_property<Mizu::ShaderUniformBufferProperty>(*ub1);
            REQUIRE(ub1_prop.name == "uUniform1");
            REQUIRE(ub1_prop.total_size == 32);

            std::ranges::sort(ub1_prop.members, [](auto a, auto b) { return a.name < b.name; });

            REQUIRE(ub1_prop.members[0].type == Mizu::ShaderValueProperty::Type::Vec3);
            REQUIRE(ub1_prop.members[0].size == 4 * 3);
            REQUIRE(ub1_prop.members[0].name == "Direction");

            REQUIRE(ub1_prop.members[1].type == Mizu::ShaderValueProperty::Type::Vec4);
            REQUIRE(ub1_prop.members[1].size == 4 * 4);
            REQUIRE(ub1_prop.members[1].name == "Position");
        }

        SECTION("Graphics Shader detects if property does not exist") {
            const auto prop1 = graphics_shader->get_property("randomProperty");
            REQUIRE(!prop1.has_value());

            const auto prop2 = graphics_shader->get_property("uTexture3");
            REQUIRE(!prop2.has_value());
        }
    }

    SECTION("Compute Shader 1") {
        const auto path = ResourcesManager::get_resource_path("ComputeShader_1.Comp.spv");
        const auto compute_shader = Mizu::ComputeShader::create(path);

        SECTION("Shader compiles correctly") {
            REQUIRE(compute_shader != nullptr);
        }

        SECTION("Compute Shader has correct properties") {
            const auto properties = compute_shader->get_properties();
            REQUIRE(properties.size() == 2);

            auto input_texture = compute_shader->get_property("uInputImage");
            REQUIRE(input_texture.has_value());
            get_shader_property<Mizu::ShaderTextureProperty>(*input_texture);

            auto output_texture = compute_shader->get_property("uOutputImage");
            REQUIRE(output_texture.has_value());
            get_shader_property<Mizu::ShaderTextureProperty>(*output_texture);
        }

        SECTION("Compute Shader detects if property does not exist") {
            const auto prop1 = compute_shader->get_property("uInputImage2");
            REQUIRE(!prop1.has_value());

            const auto prop2 = compute_shader->get_property("uoutputImage");
            REQUIRE(!prop2.has_value());
        }
    }

    Mizu::shutdown();
}
