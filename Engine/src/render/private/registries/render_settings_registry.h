#pragma once

#include <bitset>
#include <glm/glm.hpp>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "render/render_settings/render_settings.h"
#include "render/state_manager/render_settings_layer_state_manager.h"
#include "render/state_manager/render_settings_volume_state_manager.h"

namespace Mizu
{

class RenderSettingsRegistry : public RenderSettingsLayerStateManagerConsumer,
                               public RenderSettingsVolumeStateManagerConsumer
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

    void update(const glm::vec3& camera_position);

    // RenderSettingsLayerStateManagerConsumer
    void rend_on_create(
        RenderSettingsLayerHandle handle,
        const RenderSettingsLayerStaticState& ss,
        const RenderSettingsLayerDynamicState& ds) override;
    void rend_on_update(RenderSettingsLayerHandle handle, const RenderSettingsLayerDynamicState& ds) override;
    void rend_on_destroy(RenderSettingsLayerHandle handle) override;

    // RenderSettingsVolumeStateManagerConsumer
    void rend_on_create(
        RenderSettingsVolumeHandle handle,
        const RenderSettingsVolumeStaticState& ss,
        const RenderSettingsVolumeDynamicState& ds) override;
    void rend_on_update(RenderSettingsVolumeHandle handle, const RenderSettingsVolumeDynamicState& ds) override;
    void rend_on_destroy(RenderSettingsVolumeHandle handle) override;

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

    struct RenderSettingsVolumeInfo
    {
        RenderSettingsVolumeHandle handle;
        RenderSettingsVolumeStaticState ss;
        RenderSettingsVolumeDynamicState ds;

        bool is_active = false;
    };

    std::vector<RenderSettingsLayerInfo> m_layers{};
    std::vector<RenderSettingsVolumeInfo> m_volumes{};

    void update_active_volumes(const glm::vec3& camera_position);

    void mark_settings_changed(const RenderSettingsLayerDynamicState& ds);
    void mark_settings_changed(const RenderSettingsVolumeDynamicState& ds);
};

void render_settings_registry_init();
void render_settings_registry_shutdown();
void render_settings_registry_update(const glm::vec3& camera_position);
RenderSettingsRegistry& render_settings_registry_get();

template <typename T>
const T& render_settings_registry_resolve()
{
    return render_settings_registry_get().resolve<T>();
}

} // namespace Mizu