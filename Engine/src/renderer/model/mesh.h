#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "base/math/bounding_box.h"
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

    struct Description
    {
        std::string name;
    };

    Mesh(Description desc, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    std::shared_ptr<VertexBuffer> vertex_buffer() const { return m_vertex_buffer; }
    std::shared_ptr<IndexBuffer> index_buffer() const { return m_index_buffer; }
    BBox bbox() const { return m_bbox; }

    const std::string& get_name() const { return m_description.name; }

  private:
    Description m_description;

    std::shared_ptr<VertexBuffer> m_vertex_buffer{nullptr};
    std::shared_ptr<IndexBuffer> m_index_buffer{nullptr};
    BBox m_bbox;

    void create_blas();
    void compute_bbox(const std::vector<Vertex>& vertices);
};

} // namespace Mizu
