#pragma once

#include <bitset>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "render/render_settings/render_settings.h"
#include "render/state_manager/render_settings_layer_state_manager.h"

namespace Mizu
{

class RenderSettingsRegistry : public RenderSettingsLayerStateManagerConsumer
{
  public:
    RenderSettingsRegistry();
    ~RenderSettingsRegistry() override;

    template <typename T>
    const T& resolve() const
    {
        static_assert(
            is_variant_alternative_v<T, AllRenderSettingsVariant>,
            "Cant't resolve a type that is not a RenderSetting, see AllRenderSettingsVariant for allowed types");

        return std::get<T>(m_resolved_settings);
    }

    void update();

    // RenderSettingsLayerStateManagerConsumer
    void rend_on_create(
        RenderSettingsLayerHandle handle,
        const RenderSettingsLayerStaticState& ss,
        const RenderSettingsLayerDynamicState& ds) override;
    void rend_on_update(RenderSettingsLayerHandle handle, const RenderSettingsLayerDynamicState& ds) override;
    void rend_on_destroy(RenderSettingsLayerHandle handle) override;

  public:
    template <typename Variant>
    struct variant_to_tuple;

    template <typename... Ts>
    struct variant_to_tuple<std::variant<Ts...>>
    {
        using type = std::tuple<Ts...>;
    };

    using RenderSettingsTuple = variant_to_tuple<AllRenderSettingsVariant>::type;

    RenderSettingsTuple m_resolved_settings{};
    std::bitset<std::variant_size_v<AllRenderSettingsVariant>> m_setting_changed{};

    struct RenderSettingsLayerInfo
    {
        RenderSettingsLayerHandle handle;
        RenderSettingsLayerDynamicState ds;
    };

    std::vector<RenderSettingsLayerInfo> m_layers;

    void mark_settings_changed(const RenderSettingsLayerDynamicState& ds);
};

void render_settings_registry_init();
void render_settings_registry_shutdown();
void render_settings_registry_update();
RenderSettingsRegistry& render_settings_registry_get();

} // namespace Mizu