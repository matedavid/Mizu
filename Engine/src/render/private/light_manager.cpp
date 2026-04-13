#include "light_manager.h"

#include "base/debug/assert.h"

#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

LightManager::LightManager()
{
    MIZU_ASSERT(g_light_state_manager != nullptr, "LightStateManager must be initialized before LightManager");
    g_light_state_manager->register_rend_consumer(this);
}

LightManager::~LightManager()
{
    if (g_light_state_manager != nullptr)
        g_light_state_manager->unregister_rend_consumer(this);
}

void LightManager::update(const Camera& camera, const CascadedShadowsSettings& shadow_settings)
{
    update_lights();
    update_cascade_shadows_data(camera, shadow_settings);
}

std::span<const GpuPointLight> LightManager::get_point_lights() const
{
    return m_point_lights;
}

std::span<const GpuDirectionalLight> LightManager::get_directional_lights() const
{
    return m_directional_lights;
}

std::span<const float> LightManager::get_cascade_splits() const
{
    return m_cascade_splits;
}

std::span<const glm::mat4> LightManager::get_cascade_light_space_matrices() const
{
    return m_cascade_light_space_matrices;
}

uint32_t LightManager::get_num_shadow_casting_directional_lights() const
{
    uint32_t num_shadow_casting_directional_lights = 0;

    for (const GpuDirectionalLight& light : m_directional_lights)
    {
        if (light.cast_shadows != 0.0f)
            num_shadow_casting_directional_lights += 1;
    }

    return num_shadow_casting_directional_lights;
}

void LightManager::rend_on_create(LightHandle handle, const LightStaticState& ss, const LightDynamicState& ds)
{
    LightManagerEntry entry{};
    entry.handle = handle;
    entry.transform_handle = ss.transform_handle;
    entry.ss = ss;
    entry.ds = ds;

    m_light_entries.push_back(entry);
}

void LightManager::rend_on_update(LightHandle handle, const LightDynamicState& ds)
{
    auto it = std::find_if(m_light_entries.begin(), m_light_entries.end(), [handle](const LightManagerEntry& entry) {
        return entry.handle == handle;
    });

    if (it == m_light_entries.end())
    {
        MIZU_UNREACHABLE("Light handle not found");
        return;
    }

    it->ds = ds;
}

void LightManager::rend_on_destroy(LightHandle handle)
{
    const auto new_end =
        std::remove_if(m_light_entries.begin(), m_light_entries.end(), [handle](const LightManagerEntry& entry) {
            return entry.handle == handle;
        });
    m_light_entries.erase(new_end, m_light_entries.end());
}

void LightManager::update_lights()
{
    m_point_lights.clear();
    m_directional_lights.clear();

    for (const LightManagerEntry& entry : m_light_entries)
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

void LightManager::update_cascade_shadows_data(const Camera& camera, const CascadedShadowsSettings& shadow_settings)
{
    const glm::mat4 view_proj = camera.get_projection_matrix() * camera.get_view_matrix();
    const glm::mat4 inverse_view_proj = glm::inverse(view_proj);

    m_cascade_splits.clear();
    m_cascade_light_space_matrices.clear();

    m_cascade_splits.reserve(shadow_settings.num_cascades);

    for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
    {
        const float clip_range = camera.get_zfar() - camera.get_znear();
        const float cascade_split =
            (camera.get_znear() + shadow_settings.cascade_split_factors[cascade_idx] * clip_range) * -1.0f;
        m_cascade_splits.push_back(cascade_split);
    }

    for (const GpuDirectionalLight& light : m_directional_lights)
    {
        if (light.cast_shadows == 0.0f)
            continue;

        for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
        {
            const float split_dist = shadow_settings.cascade_split_factors[cascade_idx];
            const float last_split_dist =
                cascade_idx == 0 ? 0.0f : shadow_settings.cascade_split_factors[cascade_idx - 1];

            glm::vec3 frustum_corners[8] = {
                glm::vec3(-1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, -1.0f, 1.0f),
                glm::vec3(-1.0f, -1.0f, 1.0f),
            };

            for (glm::vec3& corner : frustum_corners)
            {
                const glm::vec4 inverted_corner = inverse_view_proj * glm::vec4(corner, 1.0f);
                corner = inverted_corner / inverted_corner.w;
            }

            for (uint32_t i = 0; i < 4; ++i)
            {
                const glm::vec3 dist = frustum_corners[i + 4] - frustum_corners[i];
                frustum_corners[i + 4] = frustum_corners[i] + (dist * split_dist);
                frustum_corners[i] = frustum_corners[i] + (dist * last_split_dist);
            }

            glm::vec3 frustum_center{0.0f};
            for (const glm::vec3& corner : frustum_corners)
            {
                frustum_center += corner;
            }
            frustum_center /= 8.0f;

            float radius = 0.0f;
            for (const glm::vec3& corner : frustum_corners)
            {
                radius = glm::max(radius, glm::length(corner - frustum_center));
            }
            radius = glm::ceil(radius * 16.0f) / 16.0f;

            const glm::vec3 max_extents = glm::vec3(radius);
            const glm::vec3 min_extents = -max_extents;
            const glm::vec3 cascade_extents = max_extents - min_extents;
            const glm::vec3 camera_pos = frustum_center - light.direction * -min_extents.z;

            const glm::mat4 light_view = glm::lookAt(camera_pos, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 light_proj =
                glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, cascade_extents.z);

            m_cascade_light_space_matrices.push_back(light_proj * light_view);
        }
    }
}

LightManager* s_light_manager = nullptr;

void light_manager_init()
{
    MIZU_ASSERT(s_light_manager == nullptr, "LightManager is already initialized");
    s_light_manager = new LightManager{};
}

void light_manager_shutdown()
{
    delete s_light_manager;
    s_light_manager = nullptr;
}

void light_manager_update(const Camera& camera, const CascadedShadowsSettings& shadow_settings)
{
    LightManager& light_manager = const_cast<LightManager&>(light_manager_get());
    light_manager.update(camera, shadow_settings);
}

const LightManager& light_manager_get()
{
    MIZU_ASSERT(s_light_manager != nullptr, "LightManager is not initialized");
    return *s_light_manager;
}

} // namespace Mizu
