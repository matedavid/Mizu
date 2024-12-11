#pragma once

#include <entt/entt.hpp>

#include "core/uuid.h"
#include "scene/scene_definition.h"

namespace Mizu
{

class Entity
{
  public:
    Entity(Scene* scene, entt::entity entity) : m_scene(scene), m_entity(entity) {}

    Entity(Entity& other) : m_scene(other.m_scene), m_entity(other.m_entity) {}
    Entity(const Entity& other) : m_scene(other.m_scene), m_entity(other.m_entity) {}

    template <typename T, typename... Args>
    void add_component(const Args&... args)
    {
        m_scene->m_registry->emplace<T>(m_entity, args...);
    }

    template <typename T>
    void add_component(const T& value)
    {
        m_scene->m_registry->emplace<T>(m_entity, value);
    }

    template <typename T>
    void remove_component()
    {
        m_scene->m_registry->remove<T>(m_entity);
    }

    template <typename T>
    [[nodiscard]] bool has_component() const
    {
        return m_scene->m_registry->any_of<T>(m_entity);
    }

    template <typename T>
    [[nodiscard]] T& get_component() const
    {
        return m_scene->m_registry->get<T>(m_entity);
    }

    [[nodiscard]] entt::entity handle() const { return m_entity; }

  private:
    Scene* m_scene;
    entt::entity m_entity;
};

} // namespace Mizu
