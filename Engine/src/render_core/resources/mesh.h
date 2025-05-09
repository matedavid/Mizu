#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "core/bounding_box.h"
#include "render_core/resources/buffers.h"

namespace Mizu
{

class Mesh
{
  public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    [[nodiscard]] std::shared_ptr<VertexBuffer> vertex_buffer() const { return m_vertex_buffer; }
    [[nodiscard]] std::shared_ptr<IndexBuffer> index_buffer() const { return m_index_buffer; }
    [[nodiscard]] BBox bbox() const { return m_bbox; }

  private:
    std::shared_ptr<VertexBuffer> m_vertex_buffer;
    std::shared_ptr<IndexBuffer> m_index_buffer;
    BBox m_bbox;

    void compute_bbox(const std::vector<Vertex>& vertices);
};

} // namespace Mizu
