#include "resources/resource_event_stream.h"

namespace Mizu
{

void ResourceEventStream::reset()
{
#define EVENT_STREAMS(X) \
    X(m_renderable_events) X(m_mesh_residency_events) X(m_texture_residency_events) X(m_material_residency_events)

#define X(stream) stream.clear();

    EVENT_STREAMS(X)

#undef X

#undef EVENT_STREAMS
}

void ResourceEventStream::push_renderable_event(const RenderableEvent& event)
{
    m_renderable_events.push_back(event);
}

void ResourceEventStream::push_mesh_residency_event(const MeshResidencyEvent& event)
{
    m_mesh_residency_events.push_back(event);
}

void ResourceEventStream::push_texture_residency_event(const TextureResidencyEvent& event)
{
    m_texture_residency_events.push_back(event);
}

void ResourceEventStream::push_material_residency_event(const MaterialResidencyEvent& event)
{
    m_material_residency_events.push_back(event);
}

} // namespace Mizu