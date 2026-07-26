#include "registries/light_registry.h"

#include <algorithm>

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

LightRegistry::LightRegistry()
{
    MIZU_ASSERT(g_light_state_manager != nullptr, "LightStateManager must be initialized before LightRegistry");
    g_light_state_manager->register_rend_consumer(this);
}

LightRegistry::~LightRegistry()
{
    if (g_light_state_manager != nullptr)
        g_light_state_manager->unregister_rend_consumer(this);
}

void LightRegistry::update()
{
    MIZU_PROFILE_SCOPED;

    m_point_lights.clear();
    m_directional_lights.clear();

    for (const LightRegistryEntry& entry : m_light_entries)
    {
        const TransformDynamicState& transform_ds =
            g_transform_state_manager->rend_get_dynamic_state(entry.transform_handle);

        switch (entry.ss.type)
        {
        case LightType::Point: {
            GpuPointLight light{};
            light.position = transform_ds.translation;
            light.color = entry.ds.color;
            light.intensity = entry.ds.intensity;
            light.cast_shadows = entry.ds.cast_shadows ? 1.0f : 0.0f;
            light.radius = entry.ds.data.point.radius;

            m_point_lights.push_back(light);

            break;
        }
        case LightType::Directional: {
            GpuDirectionalLight light{};
            light.position = transform_ds.translation;
            light.color = entry.ds.color;
            light.intensity = entry.ds.intensity;
            light.cast_shadows = entry.ds.cast_shadows ? 1.0f : 0.0f;
            light.direction = entry.ds.data.directional.direction;

            m_directional_lights.push_back(light);

            break;
        }
        }
    }
}

std::span<const GpuPointLight> LightRegistry::get_point_lights() const
{
    return m_point_lights;
}

std::span<const GpuDirectionalLight> LightRegistry::get_directional_lights() const
{
    return m_directional_lights;
}

void LightRegistry::rend_on_create(LightHandle handle, const LightStaticState& ss, const LightDynamicState& ds)
{
    LightRegistryEntry entry{};
    entry.handle = handle;
    entry.transform_handle = ss.transform_handle;
    entry.ss = ss;
    entry.ds = ds;

    m_light_entries.push_back(entry);
}

void LightRegistry::rend_on_update(LightHandle handle, const LightDynamicState& ds)
{
    auto it = std::find_if(m_light_entries.begin(), m_light_entries.end(), [handle](const LightRegistryEntry& entry) {
        return entry.handle == handle;
    });

    if (it == m_light_entries.end())
    {
        MIZU_UNREACHABLE("Light handle not found");
        return;
    }

    it->ds = ds;
}

void LightRegistry::rend_on_destroy(LightHandle handle)
{
    const auto new_end =
        std::remove_if(m_light_entries.begin(), m_light_entries.end(), [handle](const LightRegistryEntry& entry) {
            return entry.handle == handle;
        });
    m_light_entries.erase(new_end, m_light_entries.end());
}

static LightRegistry* s_light_registry = nullptr;

void light_registry_init()
{
    MIZU_ASSERT(s_light_registry == nullptr, "LightRegistry is already initialized");
    s_light_registry = new LightRegistry{};
}

void light_registry_shutdown()
{
    delete s_light_registry;
    s_light_registry = nullptr;
}

void light_registry_update()
{
    LightRegistry& light_registry = light_registry_get();
    light_registry.update();
}

LightRegistry& light_registry_get()
{
    MIZU_ASSERT(s_light_registry != nullptr, "LightRegistry is not initialized");
    return *s_light_registry;
}

} // namespace Mizu
