#pragma once

#include <glm/glm.hpp>

#include "asset/asset_handle.h"
#include "state_manager/base_state_manager.h"
#include "state_manager/state_manager_consumer.h"

#include "mizu_render_module.h"
#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

struct StaticMeshStaticState
{
    TransformHandle transform_handle;
    MeshAssetHandle mesh_handle;
    MaterialAssetHandle material_handle;
};

struct StaticMeshDynamicState
{
    bool has_changed(const StaticMeshDynamicState&) const { return false; }
};

MIZU_STATE_MANAGER_CREATE_HANDLE(StaticMeshHandle);

struct StaticMeshConfig : BaseStateManagerConfig
{
    static constexpr std::string_view Identifier = "StaticMeshStateManager";
};

using StaticMeshStateManager =
    BaseStateManager<StaticMeshStaticState, StaticMeshDynamicState, StaticMeshHandle, StaticMeshConfig>;
using StaticMeshStateManagerConsumer = IStateManagerConsumer<StaticMeshStateManager>;

MIZU_RENDER_API extern StaticMeshStateManager* g_static_mesh_state_manager;

} // namespace Mizu
