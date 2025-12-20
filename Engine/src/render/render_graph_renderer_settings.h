#pragma once

#include <array>
#include <cstdint>

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

struct DebugSettings
{
    enum class DebugView
    {
        None,
        LightCulling,
        CascadedShadows,
    };

    DebugView view = DebugView::None;
};

struct RenderGraphRendererSettings
{
    CascadedShadowsSettings cascaded_shadows{};
    DebugSettings debug{};
};

} // namespace Mizu