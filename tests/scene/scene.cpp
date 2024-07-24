#include "tests_common.h"

TEST_CASE("Scene Tests", "[Scene]") {
    Mizu::Scene scene("Test Scene");

    SECTION("Can create and destroy entity") {
        const Mizu::Entity entity = scene.create_entity();
        REQUIRE(entity.get_component<Mizu::UUIDComponent>().id.is_valid());
        scene.destroy_entity(entity);
    }

    SECTION("Getting all entities with UUID component should result in getting all entities") {
        const size_t num_entities = 5;
        for (size_t i = 0; i < num_entities; ++i) {
            scene.create_entity();
        }

        const auto entities = scene.view<Mizu::UUIDComponent>();
        REQUIRE(entities.size() == num_entities);

        for (const auto& entity : entities) {
            REQUIRE(entity.get_component<Mizu::UUIDComponent>().id.is_valid());
            scene.destroy_entity(entity);
        }
    }

    struct CustomComponent {
        int x;
        float y;
    };

    SECTION("Can get all entities with custom component") {
        const size_t num_entities = 5;
        for (size_t i = 0; i < num_entities; ++i) {
            Mizu::Entity entity = scene.create_entity();
            entity.add_component<CustomComponent>(5, 1.0f);
        }

        const auto entities = scene.view<CustomComponent>();
        REQUIRE(entities.size() == num_entities);

        for (const auto& entity : entities) {
            scene.destroy_entity(entity);
        }
    }

    SECTION("Can get all entities with two components") {
        const size_t num_entities_custom = 3;
        const size_t num_entities_total = 5;

        for (size_t i = 0; i < num_entities_custom; ++i) {
            Mizu::Entity entity = scene.create_entity();
            entity.add_component<CustomComponent>(3, 1.0f);
        }

        for (size_t i = 0; i < num_entities_total - num_entities_custom; ++i) {
            scene.create_entity();
        }

        const auto custom_entities = scene.view<CustomComponent, Mizu::TransformComponent>();
        REQUIRE(custom_entities.size() == num_entities_custom);

        for (const auto& entity : scene.view<Mizu::UUIDComponent>()) {
            scene.destroy_entity(entity);
        }
    }
}