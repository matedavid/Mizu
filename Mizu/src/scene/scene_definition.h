#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <optional>
#include <string>

#include "core/uuid.h"

namespace Mizu {

// Forward declarations
class Entity;

class Scene {
  public:
    explicit Scene(std::string name);
    ~Scene();

    Entity create_entity();
    Entity create_entity(std::string name);

    void destroy_entity(Entity entity);
    void destroy_entity(UUID id);

    [[nodiscard]] std::optional<Entity> get_entity_by_id(UUID id) const;

    template <typename... Ts>
    [[nodiscard]] std::vector<Entity> view() const;

  private:
    std::string m_name;
    std::unique_ptr<entt::registry> m_registry;

    std::unordered_map<entt::entity, Entity*> m_handle_to_entity;
    std::unordered_map<UUID, Entity*> m_id_to_entity;

    void add_default_components(const entt::entity& entity, std::string name);

    friend class Entity;
};

} // namespace Mizu