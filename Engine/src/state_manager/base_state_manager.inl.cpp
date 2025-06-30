#include "base_state_manager.h"

#include "base/debug/assert.h"

namespace Mizu
{

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
BaseStateManager<StaticState, DynamicState, Handle, Config>::BaseStateManager()
{
    for (uint64_t id = 0; id < Config::MaxNumHandles; ++id)
    {
        m_available_handles.push(id);

        DynamicStateObject* start = new DynamicStateObject{};
        DynamicStateObject* current = start;
        for (uint32_t p = 0; p < Config::MaxPendingStates - 1; ++p)
        {
            DynamicStateObject* new_object = new DynamicStateObject{};

            current->next = new_object;
            new_object->prev = current;

            current = new_object;
        }

        current->next = start;
        start->prev = current;

        m_handles_dynamic_state[id] = start;
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
BaseStateManager<StaticState, DynamicState, Handle, Config>::~BaseStateManager()
{
    for (uint64_t id = 0; id < Config::MaxNumHandles; ++id)
    {
        DynamicStateObject* current = m_handles_dynamic_state[id];
        DynamicStateObject* next = current->next;

        for (uint32_t p = 0; p < Config::MaxPendingStates; ++p)
        {
            delete current;
            next = next->next;
            current = next;
        }
    }
}

//
// Sim side functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_begin_tick()
{
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_end_tick()
{
    for (const auto& [id, rend_acknowledged] : m_release_requests)
    {
        if (rend_acknowledged)
        {
            sim_release_handle(Handle(id));
        }
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
Handle BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_create_handle(StaticState static_state,
                                                                                      DynamicState dynamic_state)
{
    MIZU_ASSERT(!m_available_handles.empty(), "There are no available handles");

    const uint64_t id = m_available_handles.top();
    m_available_handles.pop();

    m_handles_static_state[id] = static_state;

    DynamicStateObject* object = m_handles_dynamic_state[id];
    object->dynamic_state = dynamic_state;
    object->rend_consumed = false;
    object->has_value = true;

    return Handle(id);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_release_handle(Handle handle)
{
    sim_mark_handle_for_release(handle);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_update(Handle handle, DynamicState dynamic_state)
{
    const uint64_t id = handle.get_internal_id();

    DynamicStateObject* current = m_handles_dynamic_state[id];

    if (!current->has_value)
    {
        current->dynamic_state = dynamic_state;
        current->rend_consumed = false;
        current->has_value = true;

        return;
    }

    uint32_t pending = 0;
    while (pending < Config::MaxPendingStates && current->has_value && current->rend_consumed)
    {
        current = current->next;

        pending += 1;
    }

    if (!current->has_value)
    {
        current->dynamic_state = dynamic_state;
        current->rend_consumed = false;
        current->has_value = true;

        m_handles_dynamic_state[id] = current;

        return;
    }

    const auto is_available = [](const DynamicStateObject* obj) {
        return !obj->has_value || (obj->has_value && obj->rend_consumed);
    };

    DynamicStateObject* available = current->next;
    while (available != current && !is_available(available))
    {
        available = available->next;
    }

    if (available == current)
    {
        available = current->prev;
    }

    available->dynamic_state = dynamic_state;
    available->rend_consumed = false;
    available->has_value = true;

    if (current->rend_consumed)
    {
        m_handles_dynamic_state[id] = available;
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
StaticState BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_get_static_state(Handle handle) const
{
    return m_handles_static_state[handle.get_internal_id()];
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_get_dynamic_state(Handle handle) const
{
    const uint64_t id = handle.get_internal_id();

    DynamicStateObject* current = m_handles_dynamic_state[id];

    const auto is_available = [](const DynamicStateObject* obj) {
        return !obj->has_value || (obj->has_value && obj->rend_consumed);
    };

    DynamicStateObject* tmp = current->next;
    while (tmp != current && !is_available(tmp))
    {
        tmp = tmp->next;
    }

    return tmp->prev->dynamic_state;
}

//
// Render side functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_begin_frame()
{
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_end_frame()
{
    for (const auto& [id, acknowledged] : m_release_requests)
    {
        if (!acknowledged)
        {
            rend_acknowledge_release(id);
        }
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
StaticState BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_get_static_state(Handle handle) const
{
    // TODO: Probably not a good idea
    return sim_get_static_state(handle);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_get_dynamic_state(Handle handle)
{
    DynamicStateObject* object = m_handles_dynamic_state[handle.get_internal_id()];
    MIZU_ASSERT(object->has_value, "DynamicState object must have at least an initial value");

    object->rend_consumed = true;

    return object->dynamic_state;
}

//
// Helpers
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_mark_handle_for_release(Handle handle)
{
    m_release_requests.insert({handle.get_internal_id(), false});
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_release_handle_internal(Handle handle)
{
    const uint64_t id = handle.get_internal_id();
    m_available_handles.push(id);

    DynamicStateObject* current = m_handles_dynamic_state[id];
    for (uint32_t i = 0; i < Config::MaxPendingStates; ++i)
    {
        current->has_value = false;
        current = current->next;
    }

    auto it = m_release_requests.find(id);
    m_release_requests.erase(it);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_acknowledge_release(Handle handle)
{
    auto it = m_release_requests.find(handle.get_internal_id());
    it->second = true;
}

} // namespace Mizu