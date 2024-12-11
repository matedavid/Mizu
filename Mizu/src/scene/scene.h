#pragma once

#include "scene/scene_definition.h"

#include "scene/components.h"
#include "scene/entity.h"

namespace Mizu
{

template <typename... Ts>
[[nodiscard]] std::vector<Entity> Scene::view() const
{
    std::vector<Entity> entities;

    const auto view = m_registry->view<Ts...>();
    for (const entt::entity& e : view)
    {
        entities.push_back(*m_handle_to_entity.find(e)->second);
    }

    return entities;
}

} // namespace Mizu
