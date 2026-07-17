#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "base/containers/inplace_vector.h"

#include "render/core/camera.h"
#include "render/core/lights.h"
#include "render/state_manager/light_state_manager.h"

namespace Mizu
{

struct CascadedShadowsSettings
{
    static constexpr uint32_t MAX_NUM_CASCADES = 10;
    static constexpr uint32_t MIN_RESOLUTION = 64;

    uint32_t resolution = 2048;

    uint32_t num_cascades = 4;
    std::array<float, MAX_NUM_CASCADES> cascade_split_factors =
        {0.05f, 0.15f, 0.50f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};

class LightRegistry : public LightStateManagerConsumer
{
  public:
    LightRegistry();
    ~LightRegistry() override;

    LightRegistry(const LightRegistry&) = delete;
    LightRegistry& operator=(const LightRegistry&) = delete;

    void update(const Camera& camera, const CascadedShadowsSettings& shadow_settings);

    std::span<const GpuPointLight> get_point_lights() const;
    std::span<const GpuDirectionalLight> get_directional_lights() const;

    std::span<const float> get_cascade_splits() const;
    std::span<const glm::mat4> get_cascade_light_space_matrices() const;
    uint32_t get_num_shadow_casting_directional_lights() const;

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

    std::vector<float> m_cascade_splits;
    std::vector<glm::mat4> m_cascade_light_space_matrices;

    void update_lights();
    void update_cascade_shadows_data(const Camera& camera, const CascadedShadowsSettings& shadow_settings);
};

void light_registry_init();
void light_registry_shutdown();
void light_registry_update(const Camera& camera, const CascadedShadowsSettings& shadow_settings);
LightRegistry& light_registry_get();

} // namespace Mizu
