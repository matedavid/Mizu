#pragma once

namespace Mizu
{

template <typename T>
concept BaseStateManagerConcept = requires {
    typename T::HandleT;
    typename T::StaticStateT;
    typename T::DynamicStateT;
};

template <typename StateManager>
class IStateManagerConsumer
{
    static_assert(
        BaseStateManagerConcept<StateManager>,
        "Templated type must define HandleT, StaticStateT and DynamicStateT types");

    using Handle = typename StateManager::HandleT;
    using StaticState = typename StateManager::StaticStateT;
    using DynamicState = typename StateManager::DynamicStateT;

  public:
    virtual void rend_on_create(Handle handle, const StaticState& ss, const DynamicState& ds) = 0;
    virtual void rend_on_update(Handle handle, const DynamicState& ds) = 0;
    virtual void rend_on_destroy(Handle handle) = 0;
};

} // namespace Mizu