#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "base/containers/inplace_vector.h"

#include "render/core/lights.h"
#include "render/state_manager/light_state_manager.h"

namespace Mizu
{

class LightRegistry : public LightStateManagerConsumer
{
  public:
    LightRegistry();
    ~LightRegistry() override;

    LightRegistry(const LightRegistry&) = delete;
    LightRegistry& operator=(const LightRegistry&) = delete;

    void update();

    std::span<const GpuPointLight> get_point_lights() const;
    std::span<const GpuDirectionalLight> get_directional_lights() const;

    void rend_on_create(LightHandle handle, const LightStaticState& ss, const LightDynamicState& ds) override;
    void rend_on_update(LightHandle handle, const LightDynamicState& ds) override;
    void rend_on_destroy(LightHandle handle) override;

  private:
    struct LightRegistryEntry
    {
        LightHandle handle;
        TransformHandle transform_handle;

        LightStaticState ss;
        LightDynamicState ds;
    };

    inplace_vector<LightRegistryEntry, LightConfig::MaxNumHandles> m_light_entries;

    inplace_vector<GpuPointLight, LightConfig::MaxNumHandles> m_point_lights;
    inplace_vector<GpuDirectionalLight, LightConfig::MaxNumHandles> m_directional_lights;
};

void light_registry_init();
void light_registry_shutdown();
void light_registry_update();
LightRegistry& light_registry_get();

} // namespace Mizu
