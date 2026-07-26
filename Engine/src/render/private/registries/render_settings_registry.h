#pragma once

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

struct RenderViewRegistryEntry;

class RenderSettingsRegistry : public RenderSettingsLayerStateManagerConsumer,
                               public RenderSettingsVolumeStateManagerConsumer
{
  public:
    RenderSettingsRegistry();
    ~RenderSettingsRegistry() override;

    void update();
    ResolvedViewRenderSettings resolve_view_settings(const RenderViewRegistryEntry& view) const;

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
    };

    std::vector<RenderSettingsLayerInfo> m_layers{};
    std::vector<RenderSettingsVolumeInfo> m_volumes{};
};

void render_settings_registry_init();
void render_settings_registry_shutdown();
void render_settings_registry_update();
RenderSettingsRegistry& render_settings_registry_get();

} // namespace Mizu
