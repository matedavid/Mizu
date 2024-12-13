#pragma once

#include <string>
#include <unordered_map>

#include "utility/assert.h"

namespace Mizu
{

class RenderGraphBlackboard
{
  public:
    ~RenderGraphBlackboard()
    {
        for (const auto& [_, value] : m_resources)
        {
            delete value;
        }
    }

    template <typename T>
    T& set()
    {
        return set<T>(T{});
    }

    template <typename T>
    T& set(T value)
    {
        if (contains<T>())
        {
            MIZU_LOG_WARNING("Blackboard resource with id {} already exists", get_id<T>());
            return get<T>();
        }

        T* v = new T;
        *v = value;

        m_resources.insert({get_id<T>(), v});
        return get<T>();
    }

    template <typename T>
    T& get() const
    {
        const char* id = get_id<T>();
        MIZU_ASSERT(contains<T>(), "Blackboard resource with id {} does not exist", id);

        return *static_cast<T*>(m_resources.find(id)->second);
    }

    template <typename T>
    bool contains() const
    {
        return m_resources.contains(get_id<T>());
    }

  private:
    std::unordered_map<const char*, void*> m_resources;

    template <typename T>
    const char* get_id() const
    {
        return typeid(T).name();
    }
};

} // namespace Mizu