#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "renderer/abstraction/buffers.h"

namespace Mizu {

// Forward declarations
class Window;
class Texture2D;
class Semaphore;

class Presenter {
  public:
    virtual ~Presenter() = default;

    [[nodiscard]] static std::shared_ptr<Presenter> create(const std::shared_ptr<Window>& window,
                                                           const std::shared_ptr<Texture2D>& texture);

    virtual void present() = 0;
    virtual void present(const std::shared_ptr<Semaphore>& wait_semaphore) = 0;

    virtual void window_resized(uint32_t width, uint32_t height) = 0;

  protected:
    struct PresenterVertex {
        glm::vec3 position;
        glm::vec2 texture_coords;
    };
    // Rendering info
    std::vector<PresenterVertex> m_vertex_data = {
        {.position = glm::vec3(-1.0f, 1.0f, 0.0f), .texture_coords = convert_texture_coords({0.0f, 1.0f})},
        {.position = glm::vec3(1.0f, 1.0f, 0.0f), .texture_coords = convert_texture_coords({1.0f, 1.0f})},
        {.position = glm::vec3(1.0f, -1.0f, 0.0f), .texture_coords = convert_texture_coords({1.0f, 0.0f})},

        {.position = glm::vec3(1.0f, -1.0f, 0.0f), .texture_coords = convert_texture_coords({1.0f, 0.0f})},
        {.position = glm::vec3(-1.0f, -1.0f, 0.0f), .texture_coords = convert_texture_coords({0.0f, 0.0f})},
        {.position = glm::vec3(-1.0f, 1.0f, 0.0f), .texture_coords = convert_texture_coords({0.0f, 1.0f})},
    };

    std::vector<VertexBuffer::Layout> m_vertex_layout = {
        {.type = VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
        {.type = VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
    };
};

} // namespace Mizu
