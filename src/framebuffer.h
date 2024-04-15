#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

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
        std::shared_ptr<Texture2D> image;
        LoadOperation load_operation;
        StoreOperation store_operation;

        glm::vec3 clear_value;
    };

    struct Description {
        std::vector<Attachment> attachments;
        uint32_t width, height;
    };

    virtual ~Framebuffer() = default;

    [[nodiscard]] static std::shared_ptr<Framebuffer> create(const Description& desc);
};

} // namespace Mizu
