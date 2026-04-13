#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "state_manager/base_state_manager.h"
#include "state_manager/base_state_manager2.h"
#include "state_manager/state_manager_consumer.h"

#include "mizu_render_module.h"
#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

// Forward declarations
class Material;
class Mesh;

struct StaticMeshStaticState
{
    TransformHandle transform_handle;
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
};

struct StaticMeshDynamicState
{
    bool has_changed(const StaticMeshDynamicState&) const { return false; }
};

struct StaticMeshConfig : BaseStateManagerConfig
{
};

MIZU_STATE_MANAGER_CREATE_HANDLE(StaticMeshHandle);

using StaticMeshStateManager =
    BaseStateManager<StaticMeshStaticState, StaticMeshDynamicState, StaticMeshHandle, StaticMeshConfig>;

MIZU_RENDER_API extern StaticMeshStateManager* g_static_mesh_state_manager;

struct StaticMeshConfig2 : BaseStateManagerConfig2
{
    static constexpr std::string_view Identifier = "StaticMeshStateManager";
};

using StaticMeshStateManager2 =
    BaseStateManager2<StaticMeshStaticState, StaticMeshDynamicState, StaticMeshHandle, StaticMeshConfig2>;
using StaticMeshStateManagerConsumer = IStateManagerConsumer<StaticMeshStateManager2>;

MIZU_RENDER_API extern StaticMeshStateManager2* g_static_mesh_state_manager2;

} // namespace Mizu
