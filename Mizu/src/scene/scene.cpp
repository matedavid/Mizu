#include "scene.h"

#include "scene/components.h"
#include "scene/entity.h"
#include "scene_definition.h"
#include "utility/logging.h"

namespace Mizu {

Scene::Scene(std::string name) : m_name(std::move(name)) {
    m_registry = std::make_unique<entt::registry>();
}

Entity Scene::create_entity() {
    const std::string default_name = "entity_" + std::to_string(m_id_to_entity.size());
    return create_entity(default_name);
}

Entity Scene::create_entity(std::string name) {
    const auto e = m_registry->create();
    add_default_components(e, std::move(name));

    Entity entity(this, e);

    const auto id = m_registry->get<UUIDComponent>(e).id;
    m_handle_to_entity.insert({e, entity});
    m_id_to_entity.insert({id, entity});

    return entity;
}

void Scene::destroy_entity(UUID id) {
    const auto it = m_id_to_entity.find(id);
    if (it == m_id_to_entity.end()) {
        MIZU_LOG_ERROR("Entity with id {} does not exist", static_cast<UUID::Type>(id));
        return;
    }

    m_registry->destroy(it->second.handle());
    m_id_to_entity.erase(it);
}

std::optional<Entity> Scene::get_entity_by_id(UUID id) const {
    const auto it = m_id_to_entity.find(id);
    if (it == m_id_to_entity.end()) {
        return std::nullopt;
    }

    return it->second;
}

void Scene::add_default_components(const entt::entity& entity, std::string name) {
    m_registry->emplace<UUIDComponent>(entity, UUID());
    m_registry->emplace<NameComponent>(entity, std::move(name));
    m_registry->emplace<TransformComponent>(entity, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
}

} // namespace Mizu
