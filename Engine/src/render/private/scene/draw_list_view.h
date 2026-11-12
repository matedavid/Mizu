#pragma once

#include "registries/render_view_registry.h"
#include "render/scene/draw_list_system_types.h"

namespace Mizu
{

inline DrawListView create_draw_list_view(const RenderViewRegistryEntry& view)
{
    return DrawListView{
        .view_matrix = view.view_matrix,
        .proj_matrix = view.proj_matrix,
        .znear = view.camera.znear,
    };
}

} // namespace Mizu
