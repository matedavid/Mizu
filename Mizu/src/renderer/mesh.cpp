#include "mesh.h"

namespace Mizu {

static std::vector<VertexBuffer::Layout> s_vertex_layout = {
    {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
    {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
    {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
};

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    m_vertex_buffer = VertexBuffer::create(vertices, s_vertex_layout);
    m_index_buffer = IndexBuffer::create(indices);

    std::vector<glm::vec3> positions(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        positions[i] = vertices[i].position;
    }

    m_bbox = BBox(positions);
}

} // namespace Mizu
