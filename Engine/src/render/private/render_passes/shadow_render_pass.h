#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "base/containers/inplace_vector.h"

#include "render/render_graph/render_graph_types.h"
#include "render/render_settings/render_settings.h"
#include "render/scene/scene_renderer_extensions.h"
#include "render/state_manager/render_view_state_manager.h"
#include "render/systems/frame_linear_allocator.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;
struct RenderViewRegistryEntry;

class CascadedShadowModule : public SceneRenderModule
{
  public:
    struct ViewData
    {
        uint32_t num_cascades = 0;
        uint32_t num_shadow_casting_directional_lights = 0;

        inplace_vector<float, SHADOW_MAX_NUM_CASCADES> cascade_splits{};
        std::vector<glm::mat4> cascade_light_space_matrices{};
    };

    void update_view(uint32_t view_id, const RenderViewRegistryEntry& view, const ResolvedViewRenderSettings& settings);

    const ViewData& get_view_data(uint32_t view_id) const { return m_view_data[view_id]; }

  private:
    std::array<ViewData, RenderViewConfig::MaxNumHandles> m_view_data{};
};

struct CascadedShadowData
{
    RenderGraphResource cascaded_shadow_atlas{};
    FrameAllocation cascade_splits_allocation{};
    FrameAllocation cascade_light_space_matrices_allocation{};
    uint32_t num_shadow_casting_directional_lights = 0;
};

CascadedShadowData create_cascaded_shadow_data(
    RenderGraphBuilder& builder,
    FrameLinearAllocator& frame_allocator,
    const CascadedShadowModule::ViewData& view_shadows,
    const ShadowRenderSettings& settings);

void add_cascaded_shadow_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

} // namespace Mizu
