#include "tests_common.h"

TEST_CASE("Entity Tests", "[Entity]") {
    Mizu::Scene scene("Test Scene");

    SECTION("Can create entity and destroy entity") {
        const Mizu::Entity entity = scene.create_entity();
        scene.destroy_entity(entity);
    }

    SECTION("Entity creation has correct default components") {
        const std::string entity_name = "Test Entity";
        const Mizu::Entity entity = scene.create_entity(entity_name);

        REQUIRE(entity.has_component<Mizu::UUIDComponent>());
        REQUIRE(entity.has_component<Mizu::NameComponent>());
        REQUIRE(entity.has_component<Mizu::TransformComponent>());

        const Mizu::UUID id = entity.get_component<Mizu::UUIDComponent>().id;
        REQUIRE(id.is_valid());

        const std::string name = entity.get_component<Mizu::NameComponent>().name;
        REQUIRE(name == entity_name);

        const Mizu::TransformComponent& transform = entity.get_component<Mizu::TransformComponent>();
        REQUIRE(transform.position == glm::vec3(0.0f));
        REQUIRE(transform.rotation == glm::vec3(0.0f));
        REQUIRE(transform.scale == glm::vec3(1.0f));

        scene.destroy_entity(entity);
    }

    SECTION("Can add and remove custom component") {
        struct CustomComponent {
            int x;
            float y;
        };

        Mizu::Entity entity = scene.create_entity();

        entity.add_component(CustomComponent{.x = 0, .y = 1.0f});
        REQUIRE(entity.has_component<CustomComponent>());

        const CustomComponent& component = entity.get_component<CustomComponent>();
        REQUIRE(component.x == 0);
        REQUIRE(component.y == 1.0f);

        entity.remove_component<CustomComponent>();
        REQUIRE(!entity.has_component<CustomComponent>());

        scene.destroy_entity(entity);
    }

    SECTION("Can modify values of component") {
        Mizu::Entity entity_1 = scene.create_entity();
        Mizu::Entity entity_2 = scene.create_entity();

        Mizu::TransformComponent& transform_1 = entity_1.get_component<Mizu::TransformComponent>();
        Mizu::TransformComponent& transform_2 = entity_2.get_component<Mizu::TransformComponent>();

        transform_1.position = glm::vec3(1.0f, 2.0f, 3.0f);
        transform_2.scale = glm::vec3(2.0f, 2.0f, 2.0f);

        REQUIRE(entity_1.get_component<Mizu::TransformComponent>().position == glm::vec3(1.0f, 2.0f, 3.0f));
        REQUIRE(entity_1.get_component<Mizu::TransformComponent>().scale == glm::vec3(1.0f, 1.0f, 1.0f));

        REQUIRE(entity_2.get_component<Mizu::TransformComponent>().scale == glm::vec3(2.0f, 2.0f, 2.0f));
        REQUIRE(entity_2.get_component<Mizu::TransformComponent>().position == glm::vec3(0.0f, 0.0f, 0.0f));

        scene.destroy_entity(entity_1);
        scene.destroy_entity(entity_2);
    }
}