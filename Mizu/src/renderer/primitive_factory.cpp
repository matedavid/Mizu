#include "primitive_factory.h"

#include <vector>

namespace Mizu {

static std::shared_ptr<Mesh> s_cube = nullptr;
static std::shared_ptr<Mesh> s_pyramid = nullptr;

void PrimitiveFactory::clean() {
    s_cube = nullptr;
    s_pyramid = nullptr;
}

std::shared_ptr<Mesh> PrimitiveFactory::get_cube() {
    if (s_cube == nullptr) {
        // clang-format off
        const std::vector<Mesh::Vertex> vertices = {
            // Front face
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, -0.5f, -1.0f}, {1.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, -0.5f, -1.0f}, {0.0f, 1.0f}},

            // Back face
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},

            // Right face
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            // Left face
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            // Top face
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},

            // Bottom face
            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        };

        const std::vector<uint32_t> indices = {
            // Front face
            1, 0, 2, 2, 3, 1,
            // Back face
            4, 5, 7, 7, 6, 4,
            // Right face
            8, 9, 11, 11, 10, 8,
            // Left face
            12, 13, 15, 15, 14, 12,
            // Top face
            16, 17, 19, 19, 18, 16,
            // Bottom face
            21, 20, 22, 22, 23, 21,
        };
        // clang-format on

        s_cube = std::make_shared<Mesh>(vertices, indices);
    }

    return s_cube;
}

std::shared_ptr<Mesh> PrimitiveFactory::get_pyramid() {
    if (s_pyramid == nullptr)
    {
        // clang-format off
        const std::vector<Mesh::Vertex> vertices = {
            // Top vertex
            {{0.0f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 1.0f}},
            // Left front vertex
            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            // Right front vertex
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
            // Left back vertex
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            // Right back vertex
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        };

        const std::vector<uint32_t> indices = {
            // Front face
            1, 2, 0,
            // Left face
            3, 1, 0,
            // Right face
            2, 4, 0,
            // Back face
            4, 3, 0,
        };
        // clang-format on

        s_pyramid = std::make_shared<Mesh>(vertices, indices);
    }

    return s_pyramid;
}

} // namespace Mizu
