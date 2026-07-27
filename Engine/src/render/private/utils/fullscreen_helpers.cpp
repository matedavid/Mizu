#include "render/utils/fullscreen_helpers.h"

#include <array>
#include <glm/glm.hpp>

#include "render_core/rhi/buffer_resource.h"

#include "render/runtime/renderer.h"
#include "render/utils/buffer_utils.h"

namespace Mizu
{

namespace FullscreenHelpers
{

struct Vertex
{
    glm::vec3 position;
    glm::vec2 tex_coord;
};

static std::shared_ptr<BufferResource> s_triangle_buffer;
static std::shared_ptr<BufferResource> s_quad_buffer;

bool init()
{
    {
        // clang-format off
        const std::array<Vertex, 3> vertex_data = {
            Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f,  1.0f}},
            Vertex{{ 3.0f, -1.0f, 0.0f}, {2.0f,  1.0f}},
            Vertex{{-1.0f,  3.0f, 0.0f}, {0.0f, -1.0f}},
        };
        // clang-format on

        s_triangle_buffer =
            BufferUtils::create_vertex_buffer(std::span<const Vertex>(vertex_data), "FullscreenTriangle");
    }

    {
        // clang-format off
        const std::array<Vertex, 6> vertex_data = {
            Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            Vertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
            Vertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
            Vertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
            Vertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
            Vertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
        };
        // clang-format on

        s_quad_buffer = BufferUtils::create_vertex_buffer(std::span<const Vertex>(vertex_data), "FullscreenQuad");
    }

    return s_triangle_buffer != nullptr && s_quad_buffer != nullptr;
}

void shutdown()
{
    s_triangle_buffer.reset();
    s_quad_buffer.reset();
}

void draw_fullscreen_triangle(CommandBuffer& command)
{
    command.bind_vertex_buffer(*s_triangle_buffer);
    command.draw(3, 0);
}

void draw_fullscreen_quad(CommandBuffer& command)
{
    command.bind_vertex_buffer(*s_quad_buffer);
    command.draw(6, 0);
}

} // namespace FullscreenHelpers
} // namespace Mizu
