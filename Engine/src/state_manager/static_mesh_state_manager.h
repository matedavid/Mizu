#pragma once

#include <glm/glm.hpp>

#include "state_manager/base_state_manager.h"
#include "state_manager/transform_state_manager.h"

namespace Mizu
{

// Forward declarations
class Material;
class Mesh;
class VertexBuffer;
class IndexBuffer;

struct StaticMeshStaticState
{
    TransformHandle transform_handle;
    // std::shared_ptr<Mesh> mesh;
    // std::shared_ptr<Material> material;

    std::shared_ptr<VertexBuffer> vb;
    std::shared_ptr<IndexBuffer> ib;
};

struct StaticMeshDynamicState
{
};

struct StaticMeshConfig : StateManagerConfig
{
};

MIZU_STATE_MANAGER_CREATE_HANDLE(StaticMeshHandle);

using StaticMeshStateManager =
    BaseStateManager<StaticMeshStaticState, StaticMeshDynamicState, StaticMeshHandle, StaticMeshConfig>;

extern StaticMeshStateManager* g_static_mesh_state_manager;

} // namespace Mizu
