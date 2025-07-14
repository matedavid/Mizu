#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "render_core/rhi/image_resource.h"

namespace Mizu
{

// Forward declarations
class ImageResourceView;

enum class LoadOperation
{
    Load,
    Clear,
    DontCare,
};

enum class StoreOperation
{
    Store,
    DontCare,
    None,
};

class Framebuffer
{
  public:
    struct Attachment
    {
        std::shared_ptr<ImageResourceView> image_view{};

        LoadOperation load_operation = LoadOperation::Clear;
        StoreOperation store_operation = StoreOperation::DontCare;

        ImageResourceState initial_state = ImageResourceState::Undefined;
        ImageResourceState final_state = ImageResourceState::General;

        glm::vec4 clear_value = glm::vec4(0.0f);
    };

    struct Description
    {
        uint32_t width = 0, height = 0;
        std::vector<Attachment> attachments{};

        std::string_view name = "";
    };

    virtual ~Framebuffer() = default;

    static std::shared_ptr<Framebuffer> create(const Description& desc);

    virtual std::vector<Attachment> get_attachments() const = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu
