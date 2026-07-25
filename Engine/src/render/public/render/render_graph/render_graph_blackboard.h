#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

class RenderGraphBlackboard
{
  public:
    RenderGraphBlackboard() = default;
    explicit RenderGraphBlackboard(const RenderGraphBlackboard& parent) : m_parent(&parent) {}

    template <typename T>
    T& add()
    {
        return add<T>(T{});
    }

    template <typename T>
    T& add(T&& value)
    {
        if (contains_local<T>())
        {
            MIZU_LOG_WARNING("Blackboard resource with id {} already exists in this scope", get_id<T>().name());
            return get<T>();
        }

        const auto container = std::make_shared<Container<T>>(std::move(value));

        m_resources.insert({get_id<T>(), container});
        return get<T>();
    }

    template <typename T>
    void remove()
    {
        if (!contains_local<T>())
        {
            MIZU_LOG_WARNING("Blackboard resource with id {} does not exist in this scope", get_id<T>().name());
            return;
        }

        m_resources.erase(get_id<T>());
    }

    template <typename T>
    T& get() const
    {
        const std::type_index id = get_id<T>();

        if (const auto it = m_resources.find(id); it != m_resources.end())
        {
            return std::static_pointer_cast<Container<T>>(it->second)->m_value;
        }

        MIZU_ASSERT(m_parent != nullptr, "Blackboard resource with id {} does not exist", id.name());
        return m_parent->get<T>();
    }

    template <typename T>
    bool contains() const
    {
        return contains_local<T>() || (m_parent != nullptr && m_parent->contains<T>());
    }

  private:
    const RenderGraphBlackboard* m_parent = nullptr;

    struct IContainer
    {
    };

    template <typename T>
    struct Container : public IContainer
    {
      public:
        Container(T value = {}) : m_value(value) {}

        T m_value;
    };

    std::unordered_map<std::type_index, std::shared_ptr<IContainer>> m_resources;

    template <typename T>
    bool contains_local() const
    {
        return m_resources.contains(get_id<T>());
    }

    template <typename T>
    std::type_index get_id() const
    {
        return std::type_index(typeid(T));
    }
};

} // namespace Mizu
