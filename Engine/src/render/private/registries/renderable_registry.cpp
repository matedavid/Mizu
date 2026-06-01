#include "registries/renderable_registry.h"

#include "base/debug/assert.h"

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

bool RenderableRegistry::consume_delta(RenderableRegistryDelta& delta)
{
    if (m_deltas_size == 0)
        return false;

    delta = m_deltas[m_deltas_head];
    m_deltas_head = (m_deltas_head + 1) % m_deltas.size();
    m_deltas_size -= 1;

    return true;
}

void RenderableRegistry::rend_on_create(
    StaticMeshHandle handle,
    const StaticMeshStaticState& ss,
    [[maybe_unused]] const StaticMeshDynamicState& ds)
{
    const RenderableRegistryEntry entry{
        .static_mesh_handle = handle,
        .transform_handle = ss.transform_handle,
        .mesh_handle = ss.mesh_handle,
        .material_handle = ss.material_handle,
    };

    m_entries.push_back(entry);

    add_delta({
        .type = RenderableRegistryDelta::Type::Create,
        .handle = handle,
        .mesh_handle = ss.mesh_handle,
        .material_handle = ss.material_handle,
    });
}

void RenderableRegistry::rend_on_update(StaticMeshHandle handle, [[maybe_unused]] const StaticMeshDynamicState& ds)
{
    add_delta({
        .type = RenderableRegistryDelta::Type::Update,
        .handle = handle,
        .mesh_handle = g_static_mesh_state_manager->get_static_state(handle).mesh_handle,
        .material_handle = g_static_mesh_state_manager->get_static_state(handle).material_handle,
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

    add_delta({
        .type = RenderableRegistryDelta::Type::Destroy,
        .handle = handle,
        .mesh_handle = g_static_mesh_state_manager->get_static_state(handle).mesh_handle,
        .material_handle = g_static_mesh_state_manager->get_static_state(handle).material_handle,
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

} // namespace Mizu