#include "resources_manager.h"
#include "tests_common.h"

TEST_CASE("ResourceGroup Tests", "[ResourceGroup]") {
    // const auto& [api, backend] = GENERATE_GRAPHICS_APIS();
    Mizu::Configuration config{};
    // config.graphics_api = api;
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    // config.backend_specific_config = backend;
    config.backend_specific_config = Mizu::VulkanSpecificConfiguration{};
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    REQUIRE(Mizu::initialize(config));

    SECTION("Can create ResourceGroup") {
        const auto resource_group = Mizu::ResourceGroup::create();
        REQUIRE(resource_group != nullptr);
    }

    SECTION("Can bake correct ResourceGroup") {
        const auto vertex_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.vert.spv");
        const auto fragment_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.frag.spv");

        const auto shader = Mizu::Shader::create(vertex_shader_path, fragment_shader_path);
        REQUIRE(shader != nullptr);

        {
            const auto resource_group = Mizu::ResourceGroup::create();
            REQUIRE(resource_group != nullptr);

            const auto texture = Mizu::Texture2D::create({});
            REQUIRE(texture != nullptr);

            resource_group->add_resource("uTexture1", texture);

            REQUIRE(resource_group->bake(shader, 0));
        }

        {
            const auto resource_group = Mizu::ResourceGroup::create();
            REQUIRE(resource_group != nullptr);

            const auto texture = Mizu::Texture2D::create({});
            REQUIRE(texture != nullptr);

            resource_group->add_resource("uTexture2", texture);

            struct GraphicsShader_1_UBO {
                glm::vec4 pos;
                glm::vec3 dir;
            };
            const auto ubo = Mizu::UniformBuffer::create<GraphicsShader_1_UBO>();
            REQUIRE(ubo != nullptr);

            resource_group->add_resource("uUniform1", ubo);

            REQUIRE(resource_group->bake(shader, 1));
        }
    }

    SECTION("Incomplete ResourceGroup fails") {
        const auto vertex_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.vert.spv");
        const auto fragment_shader_path = ResourcesManager::get_resource_path("GraphicsShader_1.frag.spv");

        const auto shader = Mizu::Shader::create(vertex_shader_path, fragment_shader_path);
        REQUIRE(shader != nullptr);

        const auto resource_group = Mizu::ResourceGroup::create();
        REQUIRE(resource_group != nullptr);

        const auto texture = Mizu::Texture2D::create({});
        REQUIRE(texture != nullptr);

        resource_group->add_resource("uTexture2", texture);

        REQUIRE(!resource_group->bake(shader, 1));
    }

    Mizu::shutdown();
}