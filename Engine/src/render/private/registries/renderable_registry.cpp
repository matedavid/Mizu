#include "registries/renderable_registry.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "resources/resource_event_stream.h"

namespace Mizu
{

RenderableRegistry::RenderableRegistry()
{
    MIZU_ASSERT(
        g_static_mesh_state_manager != nullptr, "StaticMeshStateManager must be initialized before RenderableRegistry");
    g_static_mesh_state_manager->register_rend_consumer(this);
}

RenderableRegistry::~RenderableRegistry()
{
    if (g_static_mesh_state_manager != nullptr)
        g_static_mesh_state_manager->unregister_rend_consumer(this);
}

void RenderableRegistry::update(ResourceEventStream& stream)
{
    // Update and publish the events in an update function instead of rend_on_create/update/destroy to avoid publishing
    // events on the state stream callback functions.

    RenderableRegistryDelta delta;
    while (consume_delta(delta))
    {
        RenderableEventType type{};
        switch (delta.type)
        {
        case RenderableRegistryDelta::Type::Create:
            type = RenderableEventType::Create;
            break;
        case RenderableRegistryDelta::Type::Update:
            type = RenderableEventType::Update;
            break;
        case RenderableRegistryDelta::Type::Destroy:
            type = RenderableEventType::Destroy;
            break;
        }

        stream.push_renderable_event({
            .type = type,
            .transform_handle = delta.transform_handle,
            .static_mesh_handle = delta.static_mesh_handle,
            .mesh_handle = delta.mesh_handle,
            .material_handle = delta.material_handle,
        });
    }
}

void RenderableRegistry::rend_on_create(
    StaticMeshHandle handle,
    const StaticMeshStaticState& ss,
    [[maybe_unused]] const StaticMeshDynamicState& ds)
{
    if (!ss.mesh_handle.is_valid() || !ss.material_handle.is_valid())
    {
        MIZU_LOG_ERROR(
            "StaticMeshHandle has invalid mesh or material handle (mesh = {}, material = {})",
            ss.mesh_handle.is_valid(),
            ss.material_handle.is_valid());

        return;
    }

    const RenderableRegistryEntry entry{
        .static_mesh_handle = handle,
        .transform_handle = ss.transform_handle,
        .mesh_handle = ss.mesh_handle,
        .material_handle = ss.material_handle,
    };

    m_entries.push_back(entry);

    add_delta({
        .type = RenderableRegistryDelta::Type::Create,
        .static_mesh_handle = handle,
        .transform_handle = ss.transform_handle,
        .mesh_handle = ss.mesh_handle,
        .material_handle = ss.material_handle,
    });
}

void RenderableRegistry::rend_on_update(StaticMeshHandle handle, [[maybe_unused]] const StaticMeshDynamicState& ds)
{
    const StaticMeshStaticState& ss = g_static_mesh_state_manager->get_static_state(handle);

    add_delta({
        .type = RenderableRegistryDelta::Type::Update,
        .static_mesh_handle = handle,
        .transform_handle = ss.transform_handle,
        .mesh_handle = ss.mesh_handle,
        .material_handle = ss.material_handle,
    });
}

void RenderableRegistry::rend_on_destroy(StaticMeshHandle handle)
{
    m_entries.erase(
        std::remove_if(
            m_entries.begin(),
            m_entries.end(),
            [handle](const RenderableRegistryEntry& entry) { return entry.static_mesh_handle == handle; }),
        m_entries.end());

    const StaticMeshStaticState& ss = g_static_mesh_state_manager->get_static_state(handle);

    add_delta({
        .type = RenderableRegistryDelta::Type::Destroy,
        .static_mesh_handle = handle,
        .transform_handle = ss.transform_handle,
        .mesh_handle = ss.mesh_handle,
        .material_handle = ss.material_handle,
    });
}

void RenderableRegistry::add_delta(RenderableRegistryDelta delta)
{
    MIZU_ASSERT(
        m_deltas_size < m_deltas.size(),
        "Renderable registry delta buffer overflow. Consider increasing StaticMeshConfig::MaxNumHandles.");

    if (m_deltas_size == m_deltas.size())
        return;

    m_deltas[m_deltas_tail] = std::move(delta);
    m_deltas_tail = (m_deltas_tail + 1) % m_deltas.size();
    m_deltas_size += 1;
}

bool RenderableRegistry::consume_delta(RenderableRegistryDelta& delta)
{
    if (m_deltas_size == 0)
        return false;

    delta = m_deltas[m_deltas_head];
    m_deltas_head = (m_deltas_head + 1) % m_deltas.size();
    m_deltas_size -= 1;

    return true;
}

static RenderableRegistry* s_renderable_registry = nullptr;

void renderable_registry_init()
{
    MIZU_ASSERT(s_renderable_registry == nullptr, "RenderableRegistry is already initialized");
    s_renderable_registry = new RenderableRegistry{};
}

void renderable_registry_shutdown()
{
    delete s_renderable_registry;
    s_renderable_registry = nullptr;
}

RenderableRegistry& renderable_registry_get()
{
    MIZU_ASSERT(s_renderable_registry != nullptr, "RenderableRegistry is not initialized");
    return *s_renderable_registry;
}

void renderable_registry_update(ResourceEventStream& stream)
{
    MIZU_ASSERT(s_renderable_registry != nullptr, "RenderableRegistry is not initialized");
    s_renderable_registry->update(stream);
}

} // namespace Mizu