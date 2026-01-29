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
    template <typename T>
    T& add()
    {
        return add<T>(T{});
    }

    template <typename T>
    T& add(T value)
    {
        if (contains<T>())
        {
            MIZU_LOG_WARNING("Blackboard resource with id {} already exists", get_id<T>().name());
            return get<T>();
        }

        const auto container = std::make_shared<Container<T>>(value);

        m_resources.insert({get_id<T>(), container});
        return get<T>();
    }

    template <typename T>
    T& get() const
    {
        const std::type_index id = get_id<T>();
        MIZU_ASSERT(contains<T>(), "Blackboard resource with id {} does not exist", id.name());

        return std::static_pointer_cast<Container<T>>(m_resources.find(id)->second)->m_value;
    }

    template <typename T>
    bool contains() const
    {
        return m_resources.contains(get_id<T>());
    }

  private:
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
    std::type_index get_id() const
    {
        return std::type_index(typeid(T));
    }
};

} // namespace Mizu