#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "renderer/abstraction/image_resource.h"

namespace Mizu {

// Forward declarations
class Texture2D;

enum class LoadOperation {
    Load,
    Clear,
    DontCare,
};

enum class StoreOperation {
    Store,
    DontCare,
    None,
};

class Framebuffer {
  public:
    struct Attachment {
        std::shared_ptr<Texture2D> image{};
        LoadOperation load_operation = LoadOperation::Clear;
        StoreOperation store_operation = StoreOperation::DontCare;

        // Render pass does automatic state transition
        ImageResourceState initial_state = ImageResourceState::Undefined;
        ImageResourceState final_state = ImageResourceState::General;

        glm::vec4 clear_value = glm::vec4(0.0f);
    };

    struct Description {
        std::vector<Attachment> attachments;
        uint32_t width, height;
    };

    virtual ~Framebuffer() = default;

    [[nodiscard]] static std::shared_ptr<Framebuffer> create(const Description& desc);

    [[nodiscard]] virtual std::vector<Attachment> get_attachments() const = 0;

    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
};

} // namespace Mizu
