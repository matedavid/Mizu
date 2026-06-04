#include "resources/streaming_planner.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

namespace Mizu
{

StreamingPlanner::StreamingPlanner(StreamingPlannerConfig config) : m_config(std::move(config)) {}

void StreamingPlanner::update(const ResourceEventStream& stream)
{
    MIZU_PROFILE_SCOPED;

    for (const RenderableEvent& event : stream.get_renderable_events())
    {
        switch (event.type)
        {
        case RenderableEventType::Create:
            consume_create_delta(event);
            break;
        case RenderableEventType::Update:
            consume_update_delta(event);
            break;
        case RenderableEventType::Destroy:
            consume_destroy_delta(event);
            break;
        }
    }
}

void StreamingPlanner::consume_create_delta(const RenderableEvent& event)
{
    MIZU_ASSERT(event.type == RenderableEventType::Create, "Invalid event type");

    m_mesh_request_queue.push({
        .type = StreamingRequestType::Load,
        .mesh_handle = event.mesh_handle,
    });

    m_material_request_queue.push({
        .type = StreamingRequestType::Load,
        .material_handle = event.material_handle,
    });
}

void StreamingPlanner::consume_update_delta([[maybe_unused]] const RenderableEvent& event)
{
    MIZU_ASSERT(event.type == RenderableEventType::Update, "Invalid event type");
}

void StreamingPlanner::consume_destroy_delta(const RenderableEvent& event)
{
    MIZU_ASSERT(event.type == RenderableEventType::Destroy, "Invalid event type");

    m_mesh_request_queue.push({
        .type = StreamingRequestType::Evict,
        .mesh_handle = event.mesh_handle,
    });

    m_material_request_queue.push({
        .type = StreamingRequestType::Evict,
        .material_handle = event.material_handle,
    });
}

} // namespace Mizu