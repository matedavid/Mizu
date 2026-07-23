#include "registries/render_settings_registry.h"

#include <algorithm>
#include <type_traits>

#include "base/debug/logging.h"

namespace Mizu
{

RenderSettingsRegistry::RenderSettingsRegistry()
{
    g_render_settings_layer_state_manager->register_rend_consumer(this);
}

RenderSettingsRegistry::~RenderSettingsRegistry()
{
    g_render_settings_layer_state_manager->unregister_rend_consumer(this);
}

void RenderSettingsRegistry::update()
{
    if (m_setting_changed.none())
    {
        return;
    }

    std::stable_sort(
        m_layers.begin(), m_layers.end(), [](const RenderSettingsLayerInfo& a, const RenderSettingsLayerInfo& b) {
            return a.ds.priority < b.ds.priority;
        });

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