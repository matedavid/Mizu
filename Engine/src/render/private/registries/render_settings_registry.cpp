#include "registries/render_settings_registry.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <type_traits>

#include "base/containers/inplace_vector.h"
#include "base/debug/logging.h"

#include "registries/render_view_registry.h"
#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

RenderSettingsRegistry::RenderSettingsRegistry()
{
    g_render_settings_layer_state_manager->register_rend_consumer(this);
    g_render_settings_volume_state_manager->register_rend_consumer(this);
}

RenderSettingsRegistry::~RenderSettingsRegistry()
{
    g_render_settings_layer_state_manager->unregister_rend_consumer(this);
    g_render_settings_volume_state_manager->unregister_rend_consumer(this);
}

void RenderSettingsRegistry::update()
{
    std::stable_sort(
        m_layers.begin(), m_layers.end(), [](const RenderSettingsLayerInfo& a, const RenderSettingsLayerInfo& b) {
            return a.ds.priority < b.ds.priority;
        });

    std::stable_sort(
        m_volumes.begin(), m_volumes.end(), [](const RenderSettingsVolumeInfo& a, const RenderSettingsVolumeInfo& b) {
            return a.ds.priority < b.ds.priority;
        });
}

static bool volume_contains(const RenderSettingsRegistry::RenderSettingsVolumeInfo& info, const glm::vec3& position)
{
    if (!info.ss.transform.is_valid())
    {
        MIZU_LOG_ERROR("Invalid transform handle");
        return false;
    }

    const TransformDynamicState& transform_ds = g_transform_state_manager->rend_get_dynamic_state(info.ss.transform);

    return std::visit(
        [&]<typename TShape>(const TShape& shape) -> bool {
            if constexpr (std::is_same_v<TShape, RenderSettingsVolumeDynamicState::Sphere>)
            {
                const glm::vec3 to_position = position - transform_ds.translation;

                return glm::dot(to_position, to_position) <= shape.radius * shape.radius;
            }
            else if constexpr (std::is_same_v<TShape, RenderSettingsVolumeDynamicState::Box>)
            {
                glm::mat4 rotation{1.0f};
                rotation = glm::rotate(rotation, transform_ds.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                rotation = glm::rotate(rotation, transform_ds.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                rotation = glm::rotate(rotation, transform_ds.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

                const glm::vec3 local_position =
                    glm::transpose(glm::mat3(rotation)) * (position - transform_ds.translation);

                return glm::all(glm::lessThanEqual(glm::abs(local_position), shape.half_extents));
            }
        },
        info.ds.shape);
}

ResolvedViewRenderSettings RenderSettingsRegistry::resolve_view_settings(const RenderViewRegistryEntry& view) const
{
    ResolvedViewRenderSettings result{};

    const glm::vec3 camera_position = view.camera.position;
    const RenderViewMask view_mask = view.mask;

    inplace_vector<bool, RenderSettingsVolumeConfig::MaxNumHandles> volume_active{};
    for (const RenderSettingsVolumeInfo& info : m_volumes)
    {
        const bool active = (info.ds.render_view_mask & view_mask) != 0 && volume_contains(info, camera_position);
        volume_active.push_back(active);
    }

    const auto resolve_setting = [&](auto& setting) {
        using T = std::decay_t<decltype(setting)>;
        using TOverride = typename T::Override;

        T final_setting = T{};

        if constexpr (TOverride::LayerOverridable)
        {
            for (const RenderSettingsLayerInfo& info : m_layers)
            {
                if ((info.ds.render_view_mask & view_mask) == 0)
                    continue;

                if (const TOverride* o = info.ds.get_component_opt<TOverride>())
                {
                    TOverride::apply(*o, final_setting);
                }
            }
        }

        if constexpr (TOverride::VolumeOverridable)
        {
            for (size_t i = 0; i < m_volumes.size(); ++i)
            {
                if (!volume_active[i])
                    continue;

                if (const TOverride* o = m_volumes[i].ds.get_component_opt<TOverride>())
                {
                    TOverride::apply(*o, final_setting);
                }
            }
        }

        setting = final_setting;
    };

    std::apply([&](auto&... settings) { (resolve_setting(settings), ...); }, result.m_resolved_settings);

    return result;
}

void RenderSettingsRegistry::rend_on_create(
    RenderSettingsLayerHandle handle,
    const RenderSettingsLayerStaticState&,
    const RenderSettingsLayerDynamicState& ds)
{
    m_layers.push_back({
        .handle = handle,
        .ds = ds,
    });
}

void RenderSettingsRegistry::rend_on_update(RenderSettingsLayerHandle handle, const RenderSettingsLayerDynamicState& ds)
{
    auto it = std::find_if(m_layers.begin(), m_layers.end(), [handle](const RenderSettingsLayerInfo& info) {
        return info.handle == handle;
    });

    if (it == m_layers.end())
    {
        MIZU_LOG_WARNING("Trying to update RenderSettingsLayerHandle that wasn't registered");
        return;
    }

    it->ds = ds;
}

void RenderSettingsRegistry::rend_on_destroy(RenderSettingsLayerHandle handle)
{
    const auto it = std::find_if(m_layers.begin(), m_layers.end(), [handle](const RenderSettingsLayerInfo& info) {
        return info.handle == handle;
    });

    if (it == m_layers.end())
    {
        MIZU_LOG_WARNING("Trying to destroy RenderSettingsLayerHandle that wasn't registered");
        return;
    }

    m_layers.erase(it);
}

void RenderSettingsRegistry::rend_on_create(
    RenderSettingsVolumeHandle handle,
    const RenderSettingsVolumeStaticState& ss,
    const RenderSettingsVolumeDynamicState& ds)
{
    m_volumes.push_back({
        .handle = handle,
        .ss = ss,
        .ds = ds,
    });
}

void RenderSettingsRegistry::rend_on_update(
    RenderSettingsVolumeHandle handle,
    const RenderSettingsVolumeDynamicState& ds)
{
    auto it = std::find_if(m_volumes.begin(), m_volumes.end(), [handle](const RenderSettingsVolumeInfo& info) {
        return info.handle == handle;
    });

    if (it == m_volumes.end())
    {
        MIZU_LOG_WARNING("Trying to update RenderSettingsVolumeHandle that wasn't registered");
        return;
    }

    it->ds = ds;
}

void RenderSettingsRegistry::rend_on_destroy(RenderSettingsVolumeHandle handle)
{
    const auto it = std::find_if(m_volumes.begin(), m_volumes.end(), [handle](const RenderSettingsVolumeInfo& info) {
        return info.handle == handle;
    });

    if (it == m_volumes.end())
    {
        MIZU_LOG_WARNING("Trying to destroy RenderSettingsVolumeHandle that wasn't registered");
        return;
    }

    m_volumes.erase(it);
}

static RenderSettingsRegistry* s_render_settings_registry = nullptr;

void render_settings_registry_init()
{
    MIZU_ASSERT(s_render_settings_registry == nullptr, "RenderSettingsRegistry is already initialized");
    s_render_settings_registry = new RenderSettingsRegistry{};
}

void render_settings_registry_shutdown()
{
    delete s_render_settings_registry;
    s_render_settings_registry = nullptr;
}

void render_settings_registry_update()
{
    RenderSettingsRegistry& registry = render_settings_registry_get();
    registry.update();
}

RenderSettingsRegistry& render_settings_registry_get()
{
    MIZU_ASSERT(s_render_settings_registry != nullptr, "RenderSettingsRegistry is not initialized");
    return *s_render_settings_registry;
}

} // namespace Mizu
