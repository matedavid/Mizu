#include "registries/render_settings_registry.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <type_traits>

#include "base/debug/logging.h"

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

void RenderSettingsRegistry::update(const glm::vec3& camera_position)
{
    std::stable_sort(
        m_layers.begin(), m_layers.end(), [](const RenderSettingsLayerInfo& a, const RenderSettingsLayerInfo& b) {
            return a.ds.priority < b.ds.priority;
        });

    std::stable_sort(
        m_volumes.begin(), m_volumes.end(), [](const RenderSettingsVolumeInfo& a, const RenderSettingsVolumeInfo& b) {
            return a.ds.priority < b.ds.priority;
        });

    update_active_volumes(camera_position);

    if (m_setting_changed.none())
    {
        return;
    }

    const auto resolve_setting = [&](auto& setting) {
        using T = std::decay_t<decltype(setting)>;
        using TOverride = T::Override;

        if (!m_setting_changed.test(variant_index_v<T, AllRenderSettingsVariant>))
        {
            return;
        }

        T final_setting = T{};

        if constexpr (TOverride::LayerOverridable)
        {
            for (const RenderSettingsLayerInfo& info : m_layers)
            {
                if (const TOverride* o = info.ds.get_component_opt<TOverride>())
                {
                    TOverride::apply(*o, final_setting);
                }
            }
        }

        if constexpr (TOverride::VolumeOverridable)
        {
            for (const RenderSettingsVolumeInfo& info : m_volumes)
            {
                if (!info.is_active)
                    continue;

                if (const TOverride* o = info.ds.get_component_opt<TOverride>())
                {
                    TOverride::apply(*o, final_setting);
                }
            }
        }

        setting = final_setting;
    };

    std::apply([&](auto&... settings) { (resolve_setting(settings), ...); }, m_resolved_settings);

    m_setting_changed.reset();
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

    mark_settings_changed(ds);
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

    // We need to mark the previous and new components, therefore we need two calls

    mark_settings_changed(it->ds);

    it->ds = ds;

    mark_settings_changed(it->ds);
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

    mark_settings_changed(it->ds);

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

    mark_settings_changed(ds);
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

    // We need to mark the previous and new components, therefore we need two calls

    mark_settings_changed(it->ds);

    it->ds = ds;

    mark_settings_changed(it->ds);
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

    mark_settings_changed(it->ds);

    m_volumes.erase(it);
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

void RenderSettingsRegistry::update_active_volumes(const glm::vec3& camera_position)
{
    for (RenderSettingsVolumeInfo& info : m_volumes)
    {
        const bool contains = volume_contains(info, camera_position);

        if (contains && !info.is_active)
        {
            info.is_active = true;
            mark_settings_changed(info.ds);
        }

        else if (!contains && info.is_active)
        {
            info.is_active = false;
            mark_settings_changed(info.ds);
        }
    }
}

void RenderSettingsRegistry::mark_settings_changed(const RenderSettingsLayerDynamicState& ds)
{
    for (const LayerComponentOverridesVariant& variant : ds.get_components())
    {
        std::visit(
            [&](const auto& o) {
                using TSetting = std::decay_t<decltype(o)>::Setting;
                m_setting_changed.set(variant_index_v<TSetting, AllRenderSettingsVariant>);
            },
            variant);
    }
}

void RenderSettingsRegistry::mark_settings_changed(const RenderSettingsVolumeDynamicState& ds)
{
    for (const VolumeComponentOverridesVariant& variant : ds.get_components())
    {
        std::visit(
            [&](const auto& o) {
                using TSetting = std::decay_t<decltype(o)>::Setting;
                m_setting_changed.set(variant_index_v<TSetting, AllRenderSettingsVariant>);
            },
            variant);
    }
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

void render_settings_registry_update(const glm::vec3& camera_position)
{
    RenderSettingsRegistry& registry = render_settings_registry_get();
    registry.update(camera_position);
}

RenderSettingsRegistry& render_settings_registry_get()
{
    MIZU_ASSERT(s_render_settings_registry != nullptr, "RenderSettingsRegistry is not initialized");
    return *s_render_settings_registry;
}

} // namespace Mizu