#include "resources/streaming_planner.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

namespace Mizu
{

StreamingPlanner::StreamingPlanner(StreamingPlannerConfig config, RenderableRegistry& renderable_registry)
    : m_config(std::move(config))
    , m_renderable_registry(renderable_registry)
{
}

void StreamingPlanner::update()
{
    MIZU_PROFILE_SCOPED;

    RenderableRegistryDelta delta;
    while (m_renderable_registry.consume_delta(delta))
    {
        switch (delta.type)
        {
        case RenderableRegistryDelta::Type::Create:
            consume_create_delta(delta);
            break;
        case RenderableRegistryDelta::Type::Update:
            consume_update_delta(delta);
            break;
        case RenderableRegistryDelta::Type::Destroy:
            consume_destroy_delta(delta);
            break;
        }
    }
}

void StreamingPlanner::consume_create_delta(const RenderableRegistryDelta& delta)
{
    MIZU_ASSERT(delta.type == RenderableRegistryDelta::Type::Create, "Invalid delta type");

    m_mesh_request_queue.push({
        .type = StreamingRequestType::Load,
        .mesh_handle = delta.mesh_handle,
    });

    m_material_request_queue.push({
        .type = StreamingRequestType::Load,
        .material_handle = delta.material_handle,
    });
}

void StreamingPlanner::consume_update_delta([[maybe_unused]] const RenderableRegistryDelta& delta)
{
    MIZU_ASSERT(delta.type == RenderableRegistryDelta::Type::Update, "Invalid delta type");
}

void StreamingPlanner::consume_destroy_delta(const RenderableRegistryDelta& delta)
{
    MIZU_ASSERT(delta.type == RenderableRegistryDelta::Type::Destroy, "Invalid delta type");

    m_mesh_request_queue.push({
        .type = StreamingRequestType::Evict,
        .mesh_handle = delta.mesh_handle,
    });

    m_material_request_queue.push({
        .type = StreamingRequestType::Evict,
        .material_handle = delta.material_handle,
    });
}

} // namespace Mizu