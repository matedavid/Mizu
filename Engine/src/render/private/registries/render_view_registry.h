#pragma once

#include <span>

#include "base/containers/inplace_vector.h"

#include "render/state_manager/render_view_state_manager.h"

namespace Mizu
{

struct RenderViewRegistryEntry
{
    RenderViewHandle handle{};

    ViewportRect viewport{};
    Camera2 camera{};
    uint32_t layer = 0;
    RenderViewMask mask = RENDER_VIEW_MASK_ALL;

    glm::mat4 view_matrix{};
    glm::mat4 proj_matrix{};
    glm::mat4 view_proj_matrix{};
    Frustum frustum{};
};

class RenderViewRegistry : public RenderViewStateManagerConsumer
{
  public:
    RenderViewRegistry();
    ~RenderViewRegistry() override;

    void rend_on_create(RenderViewHandle handle, const RenderViewStaticState& ss, const RenderViewDynamicState& ds)
        override;
    void rend_on_update(RenderViewHandle handle, const RenderViewDynamicState& ds) override;
    void rend_on_destroy(RenderViewHandle handle) override;

    std::span<const RenderViewRegistryEntry> get_views() const { return m_views; }

  private:
    inplace_vector<RenderViewRegistryEntry, RenderViewConfig::MaxNumHandles> m_views{};
};

void render_view_registry_init();
void render_view_registry_shutdown();
RenderViewRegistry& render_view_registry_get();
std::span<const RenderViewRegistryEntry> render_view_registry_get_views();

} // namespace Mizu