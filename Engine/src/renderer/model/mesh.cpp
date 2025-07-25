#include "mesh.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

Mesh::Mesh(Description desc, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : m_description(std::move(desc))
{
    m_vertex_buffer = VertexBuffer::create(vertices, Renderer::get_allocator());
    m_index_buffer = IndexBuffer::create(indices, Renderer::get_allocator());

    compute_bbox(vertices);
}

void Mesh::compute_bbox(const std::vector<Vertex>& vertices)
{
    std::vector<glm::vec3> positions(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        positions[i] = vertices[i].position;
    }

    m_bbox = BBox(positions);
}

} // namespace Mizu
